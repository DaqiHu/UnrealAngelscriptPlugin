/*
   AngelCode Scripting Library
   Copyright (c) 2003-2018 Andreas Jonsson

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any
   damages arising from the use of this software.

   Permission is granted to anyone to use this software for any
   purpose, including commercial applications, and to alter it and
   redistribute it freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you
      must not claim that you wrote the original software. If you use
      this software in a product, an acknowledgment in the product
      documentation would be appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and
      must not be misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
      distribution.

   The original version of this library can be located at:
   http://www.angelcode.com/angelscript/

   Andreas Jonsson
   andreas@angelcode.com
*/


//
// as_parser.cpp
//
// This class parses the script code and builds a tree for compilation
//
//
// I've documented the syntax in Extended BNF. You'll find it by doing a search in
// this file by "BNF:". The starting point for the script language is SCRIPT ::=.
//
// Ref: http://matt.might.net/articles/grammars-bnf-ebnf/
//
// ( )  - used for grouping
// { }  - 0 or more repetitions
// [ ]  - optional
//  |   - or
// ' '  - token
//



#include "as_parser.h"
#include "as_config.h"
#include "as_tokendef.h"
#include "as_texts.h"
#include "as_debug.h"
#include "as_sema.h"

#ifdef _MSC_VER
#pragma warning(disable:4702) // unreachable code
#endif

BEGIN_AS_NAMESPACE

struct asSDeclContextScope
{
	asCSema* sema;
	bool pushed;
	asSDeclContextScope(asCSema* inSema) : sema(inSema), pushed(false) {}
	~asSDeclContextScope()
	{
		if( pushed && sema )
		{
			sema->PopDeclContext();
		}
	}
	void PushLastActed()
	{
		if( sema == 0 )
		{
			return;
		}
		const asASTDeclId id = sema->LastActedDecl();
		if( !id.IsValid() )
		{
			return;
		}
		sema->PushDeclContext(id);
		pushed = true;
	}
	void Push(asASTDeclId id)
	{
		if( sema == 0 || !id.IsValid() )
		{
			return;
		}
		sema->PushDeclContext(id);
		pushed = true;
	}
};

static void AppendRecordBaseActionToken(
	const asCScriptNode* node,
	const asCScriptCode* script,
	asSRecordBaseSpecifierAction& action,
	bool& hasToken)
{
	if( node == 0 || script == 0 || script->code == 0 )
	{
		return;
	}
	if( node->firstChild != 0 )
	{
		for( const asCScriptNode* child = node->firstChild;
			child != 0; child = child->next )
		{
			AppendRecordBaseActionToken(child, script, action, hasToken);
		}
		return;
	}
	if( node->tokenLength == 0
		|| node->tokenPos + node->tokenLength > script->codeLength )
	{
		return;
	}
	asCString token;
	token.Assign(script->code + node->tokenPos, node->tokenLength);
	action.qualifiedName += token;
	if( !hasToken )
	{
		action.beginOffset = (asUINT)node->tokenPos;
		hasToken = true;
	}
	action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
}

static bool BuildRecordBaseSpecifierAction(
	const asCScriptNode* node,
	const asCScriptCode* script,
	asSRecordBaseSpecifierAction& action)
{
	bool hasToken = false;
	AppendRecordBaseActionToken(node, script, action, hasToken);
	return hasToken && action.qualifiedName.GetLength() != 0;
}

static asCString QualTypeActionNodeText(
	const asCScriptNode* node,
	const asCScriptCode* script)
{
	asCString text;
	if( node != 0 && script != 0 && script->code != 0
		&& node->tokenLength > 0
		&& node->tokenPos + node->tokenLength <= script->codeLength )
	{
		text.Assign(script->code + node->tokenPos, node->tokenLength);
	}
	return text;
}

static void AddQualTypeActionQualifier(
	eTokenType token,
	asDWORD& qualifiers)
{
	switch( token )
	{
	case ttConst: qualifiers |= asAST_QUAL_CONST; break;
	case ttHandleSuffix: qualifiers |= asAST_QUAL_HANDLE; break;
	case ttAmp: qualifiers |= asAST_QUAL_REFERENCE; break;
	case ttIn: qualifiers |= asAST_QUAL_IN; break;
	case ttOut: qualifiers |= asAST_QUAL_OUT; break;
	case ttInOut: qualifiers |= asAST_QUAL_INOUT; break;
	case ttPlus: qualifiers |= asAST_QUAL_AUTO_HANDLE; break;
	default: break;
	}

}

static void CollectLocalQualTypeActionQualifiers(
	const asCScriptNode* node,
	asDWORD& qualifiers)
{
	if( node == 0 )
	{
		return;
	}
	AddQualTypeActionQualifier(node->tokenType, qualifiers);
	for( const asCScriptNode* child = node->firstChild;
		child != 0; child = child->next )
	{
		// Nested data types and template-qualified scope components own their
		// qualifiers. Root collection must not promote those bits to the outer
		// QualType.
		if( child->nodeType != snDataType && child->nodeType != snScope )
		{
			AddQualTypeActionQualifier(child->tokenType, qualifiers);
		}
	}
}

static asCString FormatQualTypeActionBaseSpelling(
	const asCScriptNode* typeNode,
	const asCScriptCode* script)
{
	if( typeNode == 0 )
	{
		return asCString();
	}
	if( typeNode->firstChild == 0 )
	{
		return QualTypeActionNodeText(typeNode, script);
	}

	asCString primary;
	for( const asCScriptNode* child = typeNode->firstChild;
		child != 0; child = child->next )
	{
		if( child->nodeType == snDataType )
		{
			if( primary.GetLength() == 0 )
			{
				primary = FormatQualTypeActionBaseSpelling(child, script);
			}
		}
		else if( child->nodeType == snIdentifier && primary.GetLength() == 0 )
		{
			primary = QualTypeActionNodeText(child, script);
		}
	}
	if( primary.GetLength() == 0 )
	{
		primary = QualTypeActionNodeText(typeNode, script);
	}
	return primary;
}

static void ExtendQualTypeActionRange(
	const asCScriptNode* node,
	const asCScriptCode* script,
	asUINT& beginOffset,
	asUINT& endOffset,
	bool& hasRange)
{
	if( node == 0 || script == 0 || script->code == 0 )
	{
		return;
	}
	if( node->tokenLength > 0
		&& node->tokenPos + node->tokenLength <= script->codeLength )
	{
		const asUINT begin = (asUINT)node->tokenPos;
		const asUINT end = (asUINT)(node->tokenPos + node->tokenLength);
		if( !hasRange || begin < beginOffset )
		{
			beginOffset = begin;
		}
		if( !hasRange || end > endOffset )
		{
			endOffset = end;
		}
		hasRange = true;
	}
	for( const asCScriptNode* child = node->firstChild;
		child != 0; child = child->next )
	{
		ExtendQualTypeActionRange(
			child, script, beginOffset, endOffset, hasRange);
	}
}

static asUINT AppendQualTypeSyntaxNode(
	const asCScriptNode* typeNode,
	const asCScriptCode* script,
	asCArray<asSQualTypeSyntaxNode>& nodes);

static void AppendQualTypeScopeSegments(
	const asCScriptNode* scopeNode,
	const asCScriptCode* script,
	asCArray<asSQualTypeSyntaxNode>& nodes,
	asUINT ownerIndex)
{
	const asCScriptNode* child = scopeNode != 0
		? scopeNode->firstChild : 0;
	if( child != 0 && child->tokenType == ttScope )
	{
		nodes[ownerIndex].absoluteScope = true;
		child = child->next;
	}

	while( child != 0 )
	{
		if( child->nodeType != snIdentifier )
		{
			// ParseOptionalScope owns grammar validation. Keep an unexpected
			// recognized shape in the action as an invalid empty segment so Sema
			// fails closed instead of reconstructing source text heuristically.
			asSQualTypeSyntaxNode invalid;
			if( nodes[ownerIndex].scopeSegmentCount == 0 )
			{
				nodes[ownerIndex].firstScopeSegment = nodes.GetLength();
			}
			nodes.PushLast(invalid);
			++nodes[ownerIndex].scopeSegmentCount;
			break;
		}

		const asUINT segmentIndex = nodes.GetLength();
		asSQualTypeSyntaxNode segment;
		segment.spelling = QualTypeActionNodeText(child, script);
		bool hasRange = false;
		ExtendQualTypeActionRange(
			child, script, segment.beginOffset, segment.endOffset, hasRange);
		if( nodes[ownerIndex].scopeSegmentCount == 0 )
		{
			nodes[ownerIndex].firstScopeSegment = segmentIndex;
		}
		nodes.PushLast(segment);
		++nodes[ownerIndex].scopeSegmentCount;
		child = child->next;

		if( child != 0 && child->tokenType == ttScope )
		{
			child = child->next;
			nodes[segmentIndex].subtreeSize = 1;
			continue;
		}

		// ParseOptionalScope permits a template list only on the innermost
		// segment. Its snDataType children follow the identifier directly; the
		// closing `::` is consumed by grammar and deliberately is not recovered
		// from source text here.
		while( child != 0 && child->nodeType == snDataType )
		{
			if( nodes[segmentIndex].templateArgumentCount == 0 )
			{
				nodes[segmentIndex].firstTemplateArgument = nodes.GetLength();
			}
			bool segmentHasRange =
				nodes[segmentIndex].endOffset > nodes[segmentIndex].beginOffset;
			ExtendQualTypeActionRange(
				child, script,
				nodes[segmentIndex].beginOffset,
				nodes[segmentIndex].endOffset,
				segmentHasRange);
			AppendQualTypeSyntaxNode(child, script, nodes);
			++nodes[segmentIndex].templateArgumentCount;
			child = child->next;
		}
		nodes[segmentIndex].subtreeSize = nodes.GetLength() - segmentIndex;
	}
}

static asUINT AppendQualTypeSyntaxNode(
	const asCScriptNode* typeNode,
	const asCScriptCode* script,
	asCArray<asSQualTypeSyntaxNode>& nodes)
{
	const asUINT nodeIndex = nodes.GetLength();
	asSQualTypeSyntaxNode payload;
	payload.spelling = FormatQualTypeActionBaseSpelling(typeNode, script);
	// ParseType publishes its configured primitive width on the snDataType
	// root. Do not replace that semantic token with the lexical child token;
	// e.g. authored `float` may intentionally mean ttDouble.
	payload.primitiveToken = typeNode != 0
		? typeNode->tokenType : ttUnrecognizedToken;
	CollectLocalQualTypeActionQualifiers(typeNode, payload.qualifiers);
	bool hasRange = false;
	ExtendQualTypeActionRange(
		typeNode, script, payload.beginOffset, payload.endOffset, hasRange);
	nodes.PushLast(payload);

	for( const asCScriptNode* child = typeNode != 0
		? typeNode->firstChild : 0;
		child != 0; child = child->next )
	{
		if( child->nodeType == snScope )
		{
			AppendQualTypeScopeSegments(child, script, nodes, nodeIndex);
			break;
		}
	}

	asUINT dataTypeIndex = 0;
	for( const asCScriptNode* child = typeNode != 0
		? typeNode->firstChild : 0;
		child != 0; child = child->next )
	{
		if( child->nodeType != snDataType )
		{
			continue;
		}
		if( dataTypeIndex++ == 0 )
		{
			// The first direct snDataType is this node's nominal/primitive
			// spelling. Remaining direct data types are template arguments.
			continue;
		}
		if( nodes[nodeIndex].templateArgumentCount == 0 )
		{
			nodes[nodeIndex].firstTemplateArgument = nodes.GetLength();
		}
		AppendQualTypeSyntaxNode(child, script, nodes);
		++nodes[nodeIndex].templateArgumentCount;
	}
	nodes[nodeIndex].subtreeSize = nodes.GetLength() - nodeIndex;
	return nodeIndex;
}

asSQualTypeSyntaxAction asCParser::BuildQualTypeSyntaxAction(
	const asCScriptNode* typeNode,
	const asCScriptNode* typeMod)
{
	asSQualTypeSyntaxAction action;
	AppendQualTypeSyntaxNode(typeNode, script, action.nodes);
	if( action.nodes.GetLength() != 0 )
	{
		CollectLocalQualTypeActionQualifiers(
			typeMod, action.nodes[0].qualifiers);
		bool hasRange = action.nodes[0].endOffset > action.nodes[0].beginOffset;
		ExtendQualTypeActionRange(
			typeMod, script, action.nodes[0].beginOffset,
			action.nodes[0].endOffset, hasRange);
	}
	return action;
}

static bool BuildDeclRefExprAction(
	const asCScriptNode* node,
	const asCScriptCode* script,
	asASTDeclId owner,
	asSDeclRefExprAction& action)
{
	if( node == 0 || script == 0 || script->code == 0
		|| node->tokenLength == 0
		|| node->tokenPos + node->tokenLength > script->codeLength )
	{
		return false;
	}

	action.owner = owner;
	action.beginOffset = (asUINT)node->tokenPos;
	action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
	const asCScriptNode* scope = 0;
	const asCScriptNode* identifier = 0;
	for( const asCScriptNode* child = node->firstChild;
		child != 0; child = child->next )
	{
		if( child->nodeType == snScope )
		{
			scope = child;
		}
		else if( child->nodeType == snIdentifier )
		{
			identifier = child;
		}
	}
	action.name = QualTypeActionNodeText(identifier, script);
	if( action.name.GetLength() == 0 )
	{
		return false;
	}

	bool sawIdentifier = false;
	for( const asCScriptNode* child = scope ? scope->firstChild : 0;
		child != 0; child = child->next )
	{
		if( child->nodeType == snIdentifier )
		{
			const asCString segment = QualTypeActionNodeText(child, script);
			if( segment.GetLength() )
			{
				action.scopeSegments.PushLast(segment);
				sawIdentifier = true;
			}
		}
		else if( child->tokenType == ttScope && !sawIdentifier )
		{
			action.absoluteScope = true;
		}
	}
	return true;
}

static asSParseActionIdentity MakeParseActionIdentity(
	asCSema* sema,
	const asCScriptNode* node)
{
	return sema != 0 && node != 0
		? sema->MakeParseActionIdentity(
			static_cast<asUINT>(node->nodeType),
			static_cast<asUINT>(node->tokenPos),
			static_cast<asUINT>(node->tokenLength))
		: asSParseActionIdentity();
}

static void BindCompletedParsedDeclarationIdentity(
	asCSema* sema,
	const asCScriptNode* node,
	asASTDeclId declaration)
{
	if( sema != 0 && node != 0 && declaration.IsValid() )
	{
		sema->BindParsedDeclarationIdentity(
			MakeParseActionIdentity(sema, node), declaration);
	}
}

// Resolve one already-bound expression beneath a transparent Parser wrapper.
// This never asks Sema to interpret syntax: only copied section/kind/range
// action identities are considered, and more than one distinct descendant
// fails closed. The native nodes remain Parser-local traversal state.
static asASTExprId FindSingleExactParsedExpressionIdentity(
	asCSema* sema,
	const asCScriptNode* node)
{
	if( sema == 0 || node == 0 )
	{
		return asASTExprId();
	}
	const asASTExprId exact = sema->FindExactParsedExpressionIdentity(
		MakeParseActionIdentity(sema, node));
	if( exact.IsValid() )
	{
		return exact;
	}
	asASTExprId single;
	for( const asCScriptNode* child = node->firstChild;
		child != 0; child = child->next )
	{
		const asASTExprId candidate =
			FindSingleExactParsedExpressionIdentity(sema, child);
		if( !candidate.IsValid() )
		{
			continue;
		}
		if( single.IsValid() && single.value != candidate.value )
		{
			return asASTExprId();
		}
		single = candidate;
	}
	return single;
}

// Parser still owns the native syntax node for LEGACY compilation and error
// recovery, but only copied coordinates and exact Canonical identities cross
// into Sema. The returned StmtId is intentionally available for the upcoming
// typed Block/control assembly slices.
static asASTStmtId ActOnLeafStatementAction(
	asCSema* sema,
	asCScriptCode* script,
	const asCScriptNode* node,
	asELeafStatementActionKind kind,
	asASTExprId expression,
	bool recovered)
{
	if( sema == 0 || node == 0 )
	{
		return asASTStmtId();
	}
	asSLeafStatementAction action;
	action.kind = kind;
	action.owner = sema->CurrentDeclContext();
	action.expression = expression;
	action.beginOffset = (asUINT)node->tokenPos;
	action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
	action.recovered = recovered;
	return sema->ActOnLeafStatementAction(action, script);
}

static bool BuildConditionalExprAction(
	const asCScriptNode* node,
	const asCScriptCode* script,
	asCSema* sema,
	asSConditionalExprAction& action)
{
	if( node == 0 || script == 0 || script->code == 0
		|| node->firstChild == 0
		|| node->firstChild->next == 0
		|| node->firstChild->next->next == 0
		|| node->firstChild->next->next->next != 0
		|| node->tokenLength == 0
		|| node->tokenPos + node->tokenLength > script->codeLength )
	{
		return false;
	}
	action.condition = FindSingleExactParsedExpressionIdentity(
		sema, node->firstChild);
	action.thenExpression = FindSingleExactParsedExpressionIdentity(
		sema, node->firstChild->next);
	action.elseExpression = FindSingleExactParsedExpressionIdentity(
		sema, node->firstChild->next->next);
	if( !action.condition.IsValid()
		|| !action.thenExpression.IsValid()
		|| !action.elseExpression.IsValid() )
	{
		return false;
	}
	action.beginOffset = (asUINT)node->tokenPos;
	action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
	return true;
}

static void BindConditionalExprAction(
	asCScriptNode* node,
	asCScriptCode* script,
	asCSema* sema)
{
	if( sema == 0 )
	{
		return;
	}
	asSConditionalExprAction action;
	if( !BuildConditionalExprAction(node, script, sema, action) )
	{
		return;
	}
	const asASTExprId expression =
		sema->ActOnConditionalExprAction(action, script);
	if( expression.IsValid() )
	{
		sema->BindParsedExpressionIdentity(
			MakeParseActionIdentity(sema, node), expression);
	}
}

static bool BuildAssignExprAction(
	const asCScriptNode* node,
	const asCScriptCode* script,
	asCSema* sema,
	asSAssignExprAction& action)
{
	if( node == 0 || script == 0 || script->code == 0
		|| node->firstChild == 0
		|| node->firstChild->next == 0
		|| node->firstChild->next->next == 0
		|| node->firstChild->next->next->next != 0
		|| node->firstChild->next->nodeType != snExprOperator
		|| node->tokenLength == 0
		|| node->tokenPos + node->tokenLength > script->codeLength )
	{
		return false;
	}
	action.lhs = FindSingleExactParsedExpressionIdentity(
		sema, node->firstChild);
	action.rhs = FindSingleExactParsedExpressionIdentity(
		sema, node->firstChild->next->next);
	if( !action.lhs.IsValid() || !action.rhs.IsValid() )
	{
		return false;
	}
	action.op = QualTypeActionNodeText(node->firstChild->next, script);
	if( action.op.GetLength() == 0 )
	{
		return false;
	}
	action.beginOffset = (asUINT)node->tokenPos;
	action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
	return true;
}

static void BindAssignExprAction(
	asCScriptNode* node,
	asCScriptCode* script,
	asCSema* sema)
{
	if( sema == 0 )
	{
		return;
	}
	asSAssignExprAction action;
	if( !BuildAssignExprAction(node, script, sema, action) )
	{
		return;
	}
	const asASTExprId expression = sema->ActOnAssignExprAction(action, script);
	if( expression.IsValid() )
	{
		sema->BindParsedExpressionIdentity(
			MakeParseActionIdentity(sema, node), expression);
	}
}

static bool BuildBinaryExprAction(
	const asCScriptNode* node,
	const asCScriptCode* script,
	asCSema* sema,
	asSBinaryExprAction& action)
{
	if( node == 0 || script == 0 || script->code == 0
		|| node->firstChild == 0
		|| node->tokenLength == 0
		|| node->tokenPos + node->tokenLength > script->codeLength )
	{
		return false;
	}
	bool expectOperand = true;
	for( const asCScriptNode* child = node->firstChild;
		child != 0; child = child->next )
	{
		if( expectOperand )
		{
			if( child->nodeType == snExprOperator )
			{
				return false;
			}
			const asASTExprId operand =
				FindSingleExactParsedExpressionIdentity(sema, child);
			if( !operand.IsValid() )
			{
				return false;
			}
			action.operands.PushLast(operand);
		}
		else
		{
			if( child->nodeType != snExprOperator )
			{
				return false;
			}
			asSBinaryExprOperatorAction op;
			op.tokenType = child->tokenType;
			op.spelling = QualTypeActionNodeText(child, script);
			if( op.spelling.GetLength() == 0 )
			{
				return false;
			}
			action.operators.PushLast(op);
		}
		expectOperand = !expectOperand;
	}
	if( expectOperand || action.operands.GetLength() < 2
		|| action.operators.GetLength() + 1 != action.operands.GetLength() )
	{
		return false;
	}
	action.beginOffset = (asUINT)node->tokenPos;
	action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
	return true;
}

static void BindBinaryExprAction(
	asCScriptNode* node,
	asCScriptCode* script,
	asCSema* sema)
{
	if( sema == 0 )
	{
		return;
	}
	asSBinaryExprAction action;
	if( !BuildBinaryExprAction(node, script, sema, action) )
	{
		return;
	}
	const asASTExprId expression = sema->ActOnBinaryExprAction(action, script);
	if( expression.IsValid() )
	{
		sema->BindParsedExpressionIdentity(
			MakeParseActionIdentity(sema, node), expression);
	}
}

enum asEExprTermSemanticRoute
{
	asEXPR_TERM_SEMANTIC_NONE = 0,
	asEXPR_TERM_SEMANTIC_PURE_UNARY,
	asEXPR_TERM_SEMANTIC_STRUCTURAL_POSTFIX
};

// Classify only the semantic route; the native term remains intact for
// syntax/recovery and explicit LEGACY compilation. Prefix operators and
// postfix ++/-- form one pointer-free unary action. Dot/member, index, and
// postfix-call stages still need their own ordered action and therefore stay
// on the structural adapter until that later slice is migrated.
static asEExprTermSemanticRoute ClassifyExprTermSemanticRoute(
	const asCScriptNode* node)
{
	bool hasUnary = false;
	bool hasStructuralPostfix = false;
	for( const asCScriptNode* child = node ? node->firstChild : 0;
		child != 0; child = child->next )
	{
		if( child->nodeType == snExprPreOp )
		{
			hasUnary = true;
		}
		else if( child->nodeType == snExprPostOp )
		{
			if( child->tokenType == ttInc || child->tokenType == ttDec )
			{
				hasUnary = true;
			}
			else
			{
				hasStructuralPostfix = true;
			}
		}
	}
	if( hasStructuralPostfix )
	{
		return asEXPR_TERM_SEMANTIC_STRUCTURAL_POSTFIX;
	}
	return hasUnary
		? asEXPR_TERM_SEMANTIC_PURE_UNARY
		: asEXPR_TERM_SEMANTIC_NONE;
}

static bool BuildUnaryExprAction(
	const asCScriptNode* node,
	const asCScriptCode* script,
	asCSema* sema,
	asSUnaryExprAction& action)
{
	if( node == 0 || script == 0 || script->code == 0 || sema == 0
		|| ClassifyExprTermSemanticRoute(node)
			!= asEXPR_TERM_SEMANTIC_PURE_UNARY
		|| node->tokenLength == 0
		|| node->tokenPos + node->tokenLength > script->codeLength )
	{
		return false;
	}

	bool hasOperand = false;
	bool sawPostfix = false;
	for( const asCScriptNode* child = node->firstChild;
		child != 0; child = child->next )
	{
		if( child->nodeType == snExprPreOp )
		{
			if( hasOperand || sawPostfix )
			{
				return false;
			}
			asSUnaryExprOperatorAction op;
			op.tokenType = child->tokenType;
			op.spelling = QualTypeActionNodeText(child, script);
			if( op.spelling.GetLength() == 0 )
			{
				return false;
			}
			action.prefixOperators.PushLast(op);
		}
		else if( child->nodeType == snExprPostOp )
		{
			if( !hasOperand
				|| (child->tokenType != ttInc && child->tokenType != ttDec) )
			{
				return false;
			}
			sawPostfix = true;
			asSUnaryExprOperatorAction op;
			op.tokenType = child->tokenType;
			op.spelling = QualTypeActionNodeText(child, script);
			if( op.spelling.GetLength() == 0 )
			{
				return false;
			}
			action.postfixOperators.PushLast(op);
		}
		else
		{
			if( hasOperand || sawPostfix )
			{
				return false;
			}
			action.operand =
				FindSingleExactParsedExpressionIdentity(sema, child);
			if( !action.operand.IsValid() )
			{
				return false;
			}
			hasOperand = true;
		}
	}

	if( !hasOperand
		|| (action.prefixOperators.GetLength() == 0
			&& action.postfixOperators.GetLength() == 0) )
	{
		return false;
	}
	action.beginOffset = (asUINT)node->tokenPos;
	action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
	return true;
}

static bool BindUnaryExprAction(
	asCScriptNode* node,
	asCScriptCode* script,
	asCSema* sema)
{
	asSUnaryExprAction action;
	if( !BuildUnaryExprAction(node, script, sema, action) )
	{
		return false;
	}
	const asASTExprId expression = sema->ActOnUnaryExprAction(action, script);
	return expression.IsValid()
		&& sema->BindParsedExpressionIdentity(
			MakeParseActionIdentity(sema, node), expression);
}

static bool BuildCastExprAction(
	const asCScriptNode* node,
	const asCScriptCode* script,
	asCSema* sema,
	asSCastExprAction& action)
{
	if( node == 0 || node->nodeType != snCast || script == 0
		|| script->code == 0 || sema == 0 || node->tokenLength == 0
		|| node->tokenPos + node->tokenLength > script->codeLength )
	{
		return false;
	}

	const asCScriptNode* operandNode = 0;
	for( const asCScriptNode* child = node->firstChild;
		child != 0; child = child->next )
	{
		if( child->nodeType == snDataType )
		{
			continue;
		}
		if( operandNode != 0 )
		{
			return false;
		}
		operandNode = child;
	}
	if( operandNode == 0
		|| !sema->FindParsedExpressionType(
			MakeParseActionIdentity(sema, node), action.targetType) )
	{
		return false;
	}
	action.operand = FindSingleExactParsedExpressionIdentity(
		sema, operandNode);
	if( !action.operand.IsValid() )
	{
		return false;
	}
	action.beginOffset = (asUINT)node->tokenPos;
	action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
	return true;
}

static bool BindCastExprAction(
	asCScriptNode* node,
	asCScriptCode* script,
	asCSema* sema,
	const asCQualType& targetType)
{
	if( sema == 0 || !targetType.IsValid()
		|| !sema->BindParsedExpressionType(
			MakeParseActionIdentity(sema, node), targetType) )
	{
		return false;
	}
	asSCastExprAction action;
	if( !BuildCastExprAction(node, script, sema, action) )
	{
		return false;
	}
	const asASTExprId expression = sema->ActOnCastExprAction(action, script);
	return expression.IsValid()
		&& sema->BindParsedExpressionIdentity(
			MakeParseActionIdentity(sema, node), expression);
}

static bool BuildCallArgumentActions(
	const asCScriptNode* argsNode,
	const asCScriptCode* script,
	asCSema* sema,
	asCArray<asSCallArgumentAction>& arguments,
	bool allowTrailingRecoveryHole);

static bool BuildConstructExprAction(
	const asCScriptNode* node,
	const asCScriptCode* script,
	asCSema* sema,
	asSConstructExprAction& action)
{
	if( node == 0 || node->nodeType != snConstructCall || script == 0
		|| script->code == 0 || sema == 0 || node->tokenLength == 0
		|| node->tokenPos + node->tokenLength > script->codeLength )
	{
		return false;
	}

	const asCScriptNode* const typeNode = node->firstChild;
	const asCScriptNode* const argsNode = typeNode ? typeNode->next : 0;
	if( typeNode == 0 || typeNode->nodeType != snDataType
		|| argsNode == 0 || argsNode->nodeType != snArgList
		|| argsNode->next != 0
		|| !sema->FindParsedExpressionType(
			MakeParseActionIdentity(sema, node), action.targetType) )
	{
		return false;
	}

	asCArray<asSCallArgumentAction> arguments;
	if( !BuildCallArgumentActions(
		argsNode, script, sema, arguments, false) )
	{
		return false;
	}
	for( asUINT argumentIndex = 0;
		argumentIndex < arguments.GetLength(); ++argumentIndex )
	{
		action.arguments.PushLast(arguments[argumentIndex].expression);
		action.argumentNames.PushLast(arguments[argumentIndex].name);
	}
	action.beginOffset = (asUINT)node->tokenPos;
	action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
	return true;
}

static bool BindConstructExprAction(
	asCScriptNode* node,
	asCScriptCode* script,
	asCSema* sema,
	const asCQualType& targetType)
{
	if( sema == 0 || !targetType.IsValid()
		|| !sema->BindParsedExpressionType(
			MakeParseActionIdentity(sema, node), targetType) )
	{
		return false;
	}
	asSConstructExprAction action;
	if( !BuildConstructExprAction(node, script, sema, action) )
	{
		return false;
	}
	const asASTExprId expression =
		sema->ActOnConstructExprAction(action, script);
	return expression.IsValid()
		&& sema->BindParsedExpressionIdentity(
			MakeParseActionIdentity(sema, node), expression);
}

static bool ActionNodeHasAuthoredToken(
	const asCScriptNode* node,
	const asCScriptCode* script)
{
	if( node == 0 || script == 0 || script->code == 0 )
	{
		return false;
	}
	asCArray<const asCScriptNode*> pending;
	pending.PushLast(node);
	while( pending.GetLength() != 0 )
	{
		const asCScriptNode* const current = pending.PopLast();
		if( current->firstChild == 0 )
		{
			if( current->tokenLength > 0
				&& current->tokenPos + current->tokenLength
					<= script->codeLength )
			{
				return true;
			}
			continue;
		}
		for( const asCScriptNode* child = current->firstChild;
			child != 0; child = child->next )
		{
			pending.PushLast(child);
		}
	}
	return false;
}

static bool BuildCallArgumentActions(
	const asCScriptNode* argsNode,
	const asCScriptCode* script,
	asCSema* sema,
	asCArray<asSCallArgumentAction>& arguments,
	bool allowTrailingRecoveryHole)
{
	if( argsNode == 0 || argsNode->nodeType != snArgList
		|| script == 0 || script->code == 0 || sema == 0 )
	{
		return false;
	}
	for( const asCScriptNode* arg = argsNode->firstChild;
		arg != 0; arg = arg->next )
	{
		const asCScriptNode* expressionNode = arg;
		asSCallArgumentAction argument;
		if( arg->nodeType == snNamedArgument )
		{
			if( arg->firstChild == 0 || arg->lastChild == 0
				|| arg->firstChild == arg->lastChild
				|| arg->firstChild->nodeType != snIdentifier )
			{
				return false;
			}
			argument.name = QualTypeActionNodeText(
				arg->firstChild, script);
			if( argument.name.GetLength() == 0 )
			{
				return false;
			}
			expressionNode = arg->lastChild;
		}
		argument.expression = FindSingleExactParsedExpressionIdentity(
			sema, expressionNode);
		if( !argument.expression.IsValid() )
		{
			// ParseArgList publishes one empty trailing condition shell when the
			// source ends immediately after an opening delimiter. Preserve a
			// completed zero-argument call prefix only for that final no-token
			// recovery shell. Any authored malformed argument fails closed.
			if( allowTrailingRecoveryHole && arg->next == 0
				&& !ActionNodeHasAuthoredToken(expressionNode, script) )
			{
				break;
			}
			return false;
		}
		arguments.PushLast(argument);
	}
	return true;
}

static bool BuildCallExprAction(
	const asCScriptNode* node,
	const asCScriptCode* script,
	asCSema* sema,
	asSCallExprAction& action,
	asASTExprId implicitReceiver = asASTExprId(),
	bool allowTrailingRecoveryHole = false)
{
	if( node == 0 || node->nodeType != snFunctionCall
		|| script == 0 || script->code == 0 || sema == 0 )
	{
		return false;
	}

	asSDeclRefExprAction calleeAction;
	if( !BuildDeclRefExprAction(
		node, script, sema->CurrentDeclContext(), calleeAction) )
	{
		return false;
	}
	action.owner = calleeAction.owner;
	action.name = calleeAction.name;
	action.scopeSegments = calleeAction.scopeSegments;
	action.absoluteScope = calleeAction.absoluteScope;
	action.implicitReceiver = implicitReceiver;
	action.beginOffset = calleeAction.beginOffset;
	action.endOffset = calleeAction.endOffset;

	const asCScriptNode* argsNode = 0;
	for( const asCScriptNode* child = node->firstChild;
		child != 0; child = child->next )
	{
		if( child->nodeType != snArgList )
		{
			continue;
		}
		if( argsNode != 0 )
		{
			return false;
		}
		argsNode = child;
	}
	if( argsNode == 0 )
	{
		return false;
	}

	return BuildCallArgumentActions(
		argsNode, script, sema, action.arguments,
		allowTrailingRecoveryHole);
}

static bool BindCallExprAction(
	asCScriptNode* node,
	asCScriptCode* script,
	asCSema* sema,
	asASTExprId implicitReceiver = asASTExprId(),
	bool allowTrailingRecoveryHole = false)
{
	if( sema == 0 )
	{
		return false;
	}
	asSCallExprAction action;
	if( !BuildCallExprAction(
		node, script, sema, action, implicitReceiver,
		allowTrailingRecoveryHole) )
	{
		return false;
	}
	const asASTExprId expression =
		sema->ActOnCallExprAction(action, script);
	return expression.IsValid()
		&& sema->BindParsedExpressionIdentity(
			MakeParseActionIdentity(sema, node), expression);
}

static bool BuildStructuralPostfixExprAction(
	const asCScriptNode* node,
	const asCScriptCode* script,
	asCSema* sema,
	asSStructuralPostfixExprAction& action,
	bool allowTrailingRecoveryHole)
{
	if( node == 0 || script == 0 || script->code == 0 || sema == 0
		|| ClassifyExprTermSemanticRoute(node)
			!= asEXPR_TERM_SEMANTIC_STRUCTURAL_POSTFIX
		|| node->tokenLength == 0
		|| node->tokenPos + node->tokenLength > script->codeLength )
	{
		return false;
	}
	action.owner = sema->CurrentDeclContext();
	action.beginOffset = (asUINT)node->tokenPos;
	action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
	bool hasBase = false;
	bool sawPostfix = false;
	for( const asCScriptNode* child = node->firstChild;
		child != 0; child = child->next )
	{
		if( child->nodeType == snExprPreOp )
		{
			if( hasBase || sawPostfix )
			{
				return false;
			}
			asSUnaryExprOperatorAction op;
			op.tokenType = child->tokenType;
			op.spelling = QualTypeActionNodeText(child, script);
			if( op.spelling.GetLength() == 0 )
			{
				return false;
			}
			action.prefixOperators.PushLast(op);
			continue;
		}
		if( child->nodeType != snExprPostOp )
		{
			if( hasBase || sawPostfix )
			{
				return false;
			}
			action.base = FindSingleExactParsedExpressionIdentity(
				sema, child);
			if( !action.base.IsValid() )
			{
				return false;
			}
			hasBase = true;
			continue;
		}

		if( !hasBase )
		{
			return false;
		}
		sawPostfix = true;
		asSStructuralPostfixExprActionStep step;
		bool hasStepRange = false;
		ExtendQualTypeActionRange(
			child, script, step.beginOffset, step.endOffset,
			hasStepRange);
		if( !hasStepRange )
		{
			return false;
		}
		if( step.endOffset > action.endOffset )
		{
			action.endOffset = step.endOffset;
		}

		if( child->tokenType == ttDot && child->firstChild != 0 )
		{
			if( child->firstChild->nodeType == snFunctionCall )
			{
				asSCallExprAction call;
				if( !BuildCallExprAction(
					child->firstChild, script, sema, call,
					asASTExprId(), allowTrailingRecoveryHole) )
				{
					return false;
				}
				step.kind = asSTRUCTURAL_POSTFIX_MEMBER_CALL;
				step.name = call.name;
				step.scopeSegments = call.scopeSegments;
				step.absoluteScope = call.absoluteScope;
				step.arguments = call.arguments;
			}
			else if( child->firstChild->nodeType == snIdentifier )
			{
				step.kind = asSTRUCTURAL_POSTFIX_MEMBER;
				step.name = QualTypeActionNodeText(
					child->firstChild, script);
				if( step.name.GetLength() == 0 )
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		else if( child->tokenType == ttOpenBracket
			&& child->firstChild != 0 )
		{
			step.kind = asSTRUCTURAL_POSTFIX_INDEX;
			if( !BuildCallArgumentActions(
				child->firstChild, script, sema, step.arguments,
				allowTrailingRecoveryHole)
				|| step.arguments.GetLength() == 0 )
			{
				return false;
			}
		}
		else if( child->tokenType == ttOpenParanthesis
			&& child->firstChild != 0 )
		{
			step.kind = asSTRUCTURAL_POSTFIX_CALL;
			if( !BuildCallArgumentActions(
				child->firstChild, script, sema, step.arguments,
				allowTrailingRecoveryHole) )
			{
				return false;
			}
		}
		else if( child->tokenType == ttInc
			|| child->tokenType == ttDec )
		{
			step.kind = asSTRUCTURAL_POSTFIX_UNARY;
			step.op.tokenType = child->tokenType;
			step.op.spelling = QualTypeActionNodeText(child, script);
			if( step.op.spelling.GetLength() == 0 )
			{
				return false;
			}
		}
		else
		{
			return false;
		}
		action.steps.PushLast(step);
	}
	return hasBase && action.steps.GetLength() != 0;
}

static bool BindStructuralPostfixExprAction(
	asCScriptNode* node,
	asCScriptCode* script,
	asCSema* sema,
	bool allowTrailingRecoveryHole = false)
{
	if( sema == 0 )
	{
		return false;
	}
	asSStructuralPostfixExprAction action;
	if( !BuildStructuralPostfixExprAction(
		node, script, sema, action,
		allowTrailingRecoveryHole) )
	{
		return false;
	}
	const asASTExprId expression =
		sema->ActOnStructuralPostfixExprAction(action, script);
	return expression.IsValid()
		&& sema->BindParsedExpressionIdentity(
			MakeParseActionIdentity(sema, node), expression);
}

static bool BuildCopiedActionRange(
	const asCScriptNode* node,
	const asCScriptCode* script,
	asUINT& beginOffset,
	asUINT& endOffset)
{
	bool hasRange = false;
	ExtendQualTypeActionRange(
		node, script, beginOffset, endOffset, hasRange);
	return hasRange && endOffset > beginOffset;
}

static bool IsTrailingInitListSentinel(
	const asCScriptNode* child,
	const asCScriptCode* script)
{
	return child != 0
		&& child->nodeType == snUndefined
		&& child->next == 0
		&& script != 0
		&& script->code != 0
		&& child->tokenPos < script->codeLength
		&& script->code[child->tokenPos] == '}';
}

static asASTExprId BindInitListNodeAction(
	asCScriptNode* listNode,
	const asCScriptNode* rangeNode,
	asCScriptCode* script,
	asCSema* sema,
	const asCQualType& targetType,
	bool hasExplicitTarget,
	bool recovered)
{
	if( listNode == 0 || listNode->nodeType != snInitList
		|| rangeNode == 0 || script == 0 || sema == 0 )
	{
		return asASTExprId();
	}

	asSInitListExprAction action;
	action.owner = sema->CurrentDeclContext();
	action.targetType = targetType;
	action.hasExplicitTarget = hasExplicitTarget;
	action.recovered = recovered;
	if( !BuildCopiedActionRange(
		rangeNode, script, action.beginOffset, action.endOffset) )
	{
		return asASTExprId();
	}

	for( asCScriptNode* child = listNode->firstChild;
		child != 0; child = child->next )
	{
		if( IsTrailingInitListSentinel(child, script) )
		{
			action.hasTrailingSeparator = true;
			continue;
		}

		asSInitListElementAction element;
		if( !BuildCopiedActionRange(
			child, script, element.beginOffset, element.endOffset) )
		{
			return asASTExprId();
		}
		if( child->nodeType == snUndefined )
		{
			element.kind = asINIT_LIST_ELEMENT_OMITTED;
		}
		else
		{
			element.kind = asINIT_LIST_ELEMENT_EXPRESSION;
			if( child->nodeType == snInitList )
			{
				element.expression = BindInitListNodeAction(
					child, child, script, sema, asCQualType(), false,
					recovered);
			}
			else
			{
				element.expression =
					FindSingleExactParsedExpressionIdentity(sema, child);
			}
		}
		action.elements.PushLast(element);
	}

	const asASTExprId expression =
		sema->ActOnInitListExprAction(action, script);
	if( expression.IsValid() )
	{
		sema->BindParsedExpressionIdentity(
			MakeParseActionIdentity(sema, listNode), expression);
	}
	return expression;
}

static bool BindInitListExprAction(
	asCScriptNode* node,
	asCScriptCode* script,
	asCSema* sema,
	asCScriptNode* listNode,
	const asCQualType& targetType,
	bool hasExplicitTarget,
	bool recovered)
{
	const asASTExprId expression = BindInitListNodeAction(
		listNode, node, script, sema, targetType, hasExplicitTarget,
		recovered);
	return expression.IsValid()
		&& sema->BindParsedExpressionIdentity(
			MakeParseActionIdentity(sema, node), expression);
}

void asCParser::BuildLambdaParameterAction(
	asCScriptNode* typeNode,
	asCScriptNode* typeMod,
	asCScriptNode* nameNode,
	asCArray<asSParameterDeclAction>& actions,
	asCQualType& recoveryReturnType)
{
	if( sema == 0 || nameNode == 0 )
	{
		return;
	}

	asSParameterDeclAction action;
	action.typeOmitted = typeNode == 0;
	asSQualTypeSyntaxAction typeAction;
	if( typeNode != 0 )
	{
		typeAction = BuildQualTypeSyntaxAction(typeNode, typeMod);
		action.type = sema->ActOnQualTypeAction(typeAction, script);
	}
	if( script != 0 && script->code != 0
		&& nameNode->tokenLength > 0
		&& nameNode->tokenPos + nameNode->tokenLength <= script->codeLength )
	{
		action.name.Assign(
			script->code + nameNode->tokenPos,
			(asUINT)nameNode->tokenLength);
	}
	action.beginOffset = typeAction.nodes.GetLength() != 0
		? typeAction.nodes[0].beginOffset : (asUINT)nameNode->tokenPos;
	action.endOffset = (asUINT)(nameNode->tokenPos + nameNode->tokenLength);
	actions.PushLast(action);
	if( typeNode != 0 && !recoveryReturnType.IsValid() )
	{
		// Lambda syntax has no authored return type. Preserve the old Canonical
		// migration recovery only until contextual funcdef binding owns it.
		recoveryReturnType = action.type;
	}
}

asCParser::asCParser(asCBuilder *builder)
	: asCParser(builder ? builder->engine : 0)
{
	this->builder = builder;
}

asCParser::asCParser(asCScriptEngine *engine)
{
	this->builder    = 0;
	this->engine     = engine;

	script                = 0;
	scriptNode            = 0;
	sema                  = 0;
	checkValidTypes       = false;
	isParsingAppInterface = false;

	// FParserAccessor and internal parser clients may begin with a protected
	// ParseScript(false) entry point. Keep that first parse deterministic instead
	// of relying on the public ParseScript wrapper to initialize parser state.
	Reset();
}

asCParser::~asCParser()
{
	Reset();
}

void asCParser::Reset()
{
	errorWhileParsing     = false;
	isSyntaxError         = false;
	checkValidTypes       = false;
	isParsingAppInterface = false;

	sourcePos = 0;
	scriptNode = 0;

	script = 0;
	parsedStatementBindings.SetLength(0);

	lastToken.pos = size_t(-1);
}

void asCParser::BindParsedStatementIdentity(
	const asCScriptNode* node,
	asASTStmtId statement)
{
	if( node == 0 || !statement.IsValid() )
	{
		return;
	}
	for( asUINT i = parsedStatementBindings.GetLength(); i > 0; --i )
	{
		asSParsedStatementBinding& binding = parsedStatementBindings[i - 1];
		if( binding.node == node )
		{
			binding.statement = statement;
			return;
		}
	}
	asSParsedStatementBinding binding;
	binding.node = node;
	binding.statement = statement;
	parsedStatementBindings.PushLast(binding);
}

asASTStmtId asCParser::FindExactParsedStatementIdentity(
	const asCScriptNode* node) const
{
	if( node == 0 )
	{
		return asASTStmtId();
	}
	for( asUINT i = parsedStatementBindings.GetLength(); i > 0; --i )
	{
		const asSParsedStatementBinding& binding =
			parsedStatementBindings[i - 1];
		if( binding.node == node )
		{
			return binding.statement;
		}
	}
	return asASTStmtId();
}

asCScriptNode *asCParser::GetScriptNode()
{
	return scriptNode;
}

asASTExprId asCParser::GetParsedExpressionIdentity() const
{
	return FindSingleExactParsedExpressionIdentity(sema, scriptNode);
}

int asCParser::ParseFunctionDefinition(asCScriptCode *in_script, bool in_expectListPattern)
{
	Reset();

	// Set flag that permits ? as datatype for parameters
	isParsingAppInterface = true;

	this->script = in_script;

	scriptNode = ParseFunctionDefinition();

	if( in_expectListPattern )
		scriptNode->AddChildLast(ParseListPattern());

	// The declaration should end after the definition
	if( !isSyntaxError )
	{
		sToken t;
		GetToken(&t);
		if( t.type != ttEnd )
		{
			Error(ExpectedToken(asCTokenizer::GetDefinition(ttEnd)), &t);
			Error(InsteadFound(t), &t);
			return -1;
		}
	}

	if( errorWhileParsing )
		return -1;

	return 0;
}

asCScriptNode *asCParser::CreateNode(eScriptNode type)
{
	auto* ptr = MemStack.Alloc(sizeof(asCScriptNode), alignof(asCScriptNode));
	return new(ptr) asCScriptNode(type);
}

int asCParser::ParseDataType(asCScriptCode *in_script, bool in_isReturnType, bool isAppInterface)
{
	Reset();

	this->script = in_script;
	isParsingAppInterface = isAppInterface;

	scriptNode = CreateNode(snDataType);
	if( scriptNode == 0 ) return -1;

	scriptNode->AddChildLast(ParseType(true));
	if( isSyntaxError ) return -1;

	if( in_isReturnType )
	{
		scriptNode->AddChildLast(ParseTypeMod(false));
		if( isSyntaxError ) return -1;
	}

	// The declaration should end after the type
	sToken t;
	GetToken(&t);
	if( t.type != ttEnd )
	{
		Error(ExpectedToken(asCTokenizer::GetDefinition(ttEnd)), &t);
		Error(InsteadFound(t), &t);
		return -1;
	}

	if( errorWhileParsing )
		return -1;

	return 0;
}


// Parse a template declaration: IDENTIFIER '<' 'class'? IDENTIFIER '>'
int asCParser::ParseTemplateDecl(asCScriptCode *in_script)
{
	Reset();

	this->script = in_script;
	scriptNode = CreateNode(snUndefined);
	if( scriptNode == 0 ) return -1;

	scriptNode->AddChildLast(ParseIdentifier());
	if( isSyntaxError ) return -1;

	sToken t;
	GetToken(&t);
	if( t.type != ttLessThan )
	{
		Error(ExpectedToken(asCTokenizer::GetDefinition(ttLessThan)), &t);
		Error(InsteadFound(t), &t);
		return -1;
	}

	// The class token is optional
	GetToken(&t);
	if( t.type != ttClass )
		RewindTo(&t);

	scriptNode->AddChildLast(ParseIdentifier());
	if( isSyntaxError ) return -1;

	// There can be multiple sub types
	GetToken(&t);

	// Parse template types by list separator
	while(t.type == ttListSeparator)
	{
		GetToken(&t);
		if( t.type != ttClass )
			RewindTo(&t);
		scriptNode->AddChildLast(ParseIdentifier());

		if( isSyntaxError ) return -1;
		GetToken(&t);
	}

	if( t.type != ttGreaterThan )
	{
		Error(ExpectedToken(asCTokenizer::GetDefinition(ttGreaterThan)), &t);
		Error(InsteadFound(t), &t);
		return -1;
	}

	GetToken(&t);
	if( t.type != ttEnd )
	{
		Error(ExpectedToken(asCTokenizer::GetDefinition(ttEnd)), &t);
		Error(InsteadFound(t), &t);
		return -1;
	}

	if( errorWhileParsing )
		return -1;

	return 0;
}

int asCParser::ParsePropertyDeclaration(asCScriptCode *in_script, bool isAppInterface)
{
	Reset();

	this->script = in_script;
	isParsingAppInterface = isAppInterface;

	scriptNode = CreateNode(snDeclaration);
	if( scriptNode == 0 ) return -1;

	scriptNode->AddChildLast(ParseType(true));
	if( isSyntaxError ) return -1;

	// Allow optional '&' to indicate that the property is indirect, i.e. stored as reference
	sToken t;
	GetToken(&t);
	RewindTo(&t);
	if( t.type == ttAmp )
		scriptNode->AddChildLast(ParseToken(ttAmp));

	// Allow optional namespace to be defined before the identifier in case
	// the declaration is to be used for searching for an existing property
	ParseOptionalScope(scriptNode);

	scriptNode->AddChildLast(ParseIdentifier());
	if( isSyntaxError ) return -1;

	// The declaration should end after the identifier
	GetToken(&t);
	if( t.type != ttEnd )
	{
		Error(ExpectedToken(asCTokenizer::GetDefinition(ttEnd)), &t);
		Error(InsteadFound(t), &t);
		return -1;
	}

	return 0;
}

// BNF:5: SCOPE         ::= ['::'] {IDENTIFIER '::'} [IDENTIFIER ['<' TYPE {',' TYPE} '>'] '::']
void asCParser::ParseOptionalScope(asCScriptNode *node)
{
	asCScriptNode *scope = CreateNode(snScope);

	sToken t1, t2;
	GetToken(&t1);
	GetToken(&t2);
	if( t1.type == ttScope )
	{
		RewindTo(&t1);
		scope->AddChildLast(ParseToken(ttScope));
		GetToken(&t1);
		GetToken(&t2);
	}
	while( t1.type == ttIdentifier && t2.type == ttScope )
	{
		RewindTo(&t1);
		scope->AddChildLast(ParseIdentifier());
		scope->AddChildLast(ParseToken(ttScope));
		GetToken(&t1);
		GetToken(&t2);
	}

	// The innermost scope may be a template type
	if( t1.type == ttIdentifier && t2.type == ttLessThan )
	{
		tempString.Assign(&script->code[t1.pos], t1.length);
		if (engine->IsTemplateType(tempString.AddressOf()))
		{
			RewindTo(&t1);
			asCScriptNode *restore = scope->lastChild;
			scope->AddChildLast(ParseIdentifier());
			if (ParseTemplTypeList(scope, false))
			{
				GetToken(&t2);
				if (t2.type == ttScope)
				{
					// Template type is part of the scope
					// Nothing more needs to be done
					node->AddChildLast(scope);
					return;
				}
				else
				{
					// The template type is not part of the scope
					// Rewind to the template type and end the scope
					RewindTo(&t1);

					// Restore the previously parsed node
					while (scope->lastChild != restore)
					{
						asCScriptNode *last = scope->lastChild;
						last->DisconnectParent();
					}
					if( scope->lastChild )
						node->AddChildLast(scope);
					return;
				}
			}
		}
	}

	// The identifier is not part of the scope
	RewindTo(&t1);

	if (scope->lastChild)
		node->AddChildLast(scope);
}

asCScriptNode *asCParser::ParseFunctionDefinition(asASTDeclId* outImportDecl)
{
	asCScriptNode *node = CreateNode(snFunction);
	if( node == 0 ) return 0;
	if( outImportDecl )
	{
		*outImportDecl = asASTDeclId();
	}

	asCScriptNode* const typeNode = ParseType(true);
	node->AddChildLast(typeNode);
	if( isSyntaxError ) return node;

	asCScriptNode* const typeMod = ParseTypeMod(false);
	node->AddChildLast(typeMod);
	if( isSyntaxError ) return node;

	ParseOptionalScope(node);

	asCScriptNode* const identNode = ParseIdentifier();
	node->AddChildLast(identNode);
	if( isSyntaxError ) return node;

	// ParseImport opts into an exact declaration start before parameters are
	// recognized. ParseFunctionDefinition remains a syntax-only public helper
	// for application registration when outImportDecl is null.
	asSDeclContextScope importScope(sema);
	if( outImportDecl && sema )
	{
		asSImportSignatureAction action;
		if( identNode && script && script->code
			&& identNode->tokenLength > 0
			&& identNode->tokenPos + identNode->tokenLength <= script->codeLength )
		{
			action.name.Assign(
				script->code + identNode->tokenPos,
				(asUINT)identNode->tokenLength);
			action.beginOffset = (asUINT)identNode->tokenPos;
			action.endOffset =
				(asUINT)(identNode->tokenPos + identNode->tokenLength);
		}
		action.returnType = sema->ActOnQualTypeAction(
			BuildQualTypeSyntaxAction(typeNode, typeMod), script);
		*outImportDecl = sema->ActOnImportSignatureAction(action, script);
		importScope.PushLastActed();
	}

	node->AddChildLast(ParseParameterList(
		outImportDecl ? *outImportDecl : asASTDeclId()));
	if( isSyntaxError ) return node;

	// Parse an optional const after the function definition (used for object methods)
	sToken t1;
	GetToken(&t1);
	RewindTo(&t1);
	if( t1.type == ttConst )
		node->AddChildLast(ParseToken(ttConst));

	// Parse optional attributes
	ParseMethodAttributes(node);

	return node;
}

// BNF:4: TYPEMOD       ::= ['&' ['in' | 'out' | 'inout']]
asCScriptNode *asCParser::ParseTypeMod(bool isParam)
{
	asCScriptNode *node = CreateNode(snDataType);
	if( node == 0 ) return 0;

	sToken t;

	// Parse possible & token
	GetToken(&t);
	RewindTo(&t);
	if( t.type == ttAmp )
	{
		node->AddChildLast(ParseToken(ttAmp));
		if( isSyntaxError ) return node;

		if( isParam )
		{
			GetToken(&t);
			RewindTo(&t);

			if( t.type == ttIn || t.type == ttOut || t.type == ttInOut )
			{
				int tokens[3] = {ttIn, ttOut, ttInOut};
				node->AddChildLast(ParseOneOf(tokens, 3));
			}
		}
	}

	// Parse possible + token
	GetToken(&t);
	RewindTo(&t);
	if( t.type == ttPlus )
	{
		node->AddChildLast(ParseToken(ttPlus));
		if( isSyntaxError ) return node;
	}

	// Parse possible if_handle_then_const token
	GetToken(&t);
	RewindTo(&t);
	if (IdentifierIs(t, IF_HANDLE_TOKEN))
	{
		node->AddChildLast(ParseToken(ttIdentifier));
		if (isSyntaxError) return node;
	}
	else if (IdentifierIs(t, HANDLE_ONLY_TOKEN))
	{
		node->AddChildLast(ParseToken(ttIdentifier));
		if (isSyntaxError) return node;
	}
	else if (IdentifierIs(t, AUTO_CONSTREF_TOKEN))
	{
		node->AddChildLast(ParseToken(ttIdentifier));
		if (isSyntaxError) return node;
	}
	else if (IdentifierIs(t, ANY_IMPLICIT_INTEGER_TOKEN))
	{
		node->AddChildLast(ParseToken(ttIdentifier));
		if (isSyntaxError) return node;
	}

	return node;
}

// BNF:4: TYPE          ::= ['const'] SCOPE DATATYPE ['<' TYPE {',' TYPE} '>'] { ('[' ']') | '@' }
asCScriptNode *asCParser::ParseType(bool allowConst, bool allowVariableType, bool allowAuto)
{
	asCScriptNode *node = CreateNode(snDataType);
	if( node == 0 ) return 0;

	sToken t;

	if( allowConst )
	{
		GetToken(&t);
		RewindTo(&t);
		if( t.type == ttConst )
		{
			node->AddChildLast(ParseToken(ttConst));
			if( isSyntaxError ) return node;
		}
	}

	// Parse scope prefix
	ParseOptionalScope(node);

	// Parse the actual type
	node->AddChildLast(ParseDataType(allowVariableType, allowAuto));
	if( isSyntaxError ) return node;

	// If the datatype is a template type, then parse the subtype within the < >
	GetToken(&t);
	RewindTo(&t);
	asCScriptNode *type = node->lastChild;
	tempString.Assign(&script->code[type->tokenPos], type->tokenLength);
	if( engine->IsTemplateType(tempString.AddressOf()) && t.type == ttLessThan )
	{
		ParseTemplTypeList(node);
		if (isSyntaxError) return node;
	}

	// Parse [] and @
	GetToken(&t);
	RewindTo(&t);
	while( t.type == ttOpenBracket || t.type == ttHandleSuffix || t.type == ttUnresolvedObject)
	{
		if( t.type == ttOpenBracket )
		{
			node->AddChildLast(ParseToken(ttOpenBracket));
			if( isSyntaxError ) return node;

			GetToken(&t);
			if( t.type != ttCloseBracket )
			{
				Error(ExpectedToken("]"), &t);
				Error(InsteadFound(t), &t);
				return node;
			}
		}
		else
		{
			node->AddChildLast(ParseToken(t.type));
			if( isSyntaxError ) return node;
		}

		GetToken(&t);
		RewindTo(&t);
	}

	return node;
}

// This parses a template type list, e.g. <type, type, type>
// If 'required' is false, and the template type list is not valid,
// then no change will be done and the function returns false. This
// can be used as do an optional parsing
bool asCParser::ParseTemplTypeList(asCScriptNode *node, bool required)
{
	sToken t;
	bool isValid = true;

	// Remember the last child, so we can restore the state if needed
	asCScriptNode *last = node->lastChild;

	// Starts with '<'
	GetToken(&t);
	if (t.type != ttLessThan)
	{
		if (required)
		{
			Error(ExpectedToken(asCTokenizer::GetDefinition(ttLessThan)), &t);
			Error(InsteadFound(t), &t);
		}
		return false;
	}

	// At least one type
	// TODO: child funcdef: Make this work with !required
	node->AddChildLast(ParseType(true, false));
	if (isSyntaxError) return false;

	GetToken(&t);

	// Parse template types by list separator
	while (t.type == ttListSeparator)
	{
		// TODO: child funcdef: Make this work with !required
		node->AddChildLast(ParseType(true, false));
		if (isSyntaxError) return false;
		GetToken(&t);
	}

	// End with '>'
	// Accept >> and >>> tokens too. But then force the tokenizer to move
	// only 1 character ahead (thus splitting the token in two).
	if (script->code[t.pos] != '>')
	{
		if (required)
		{
			Error(ExpectedToken(asCTokenizer::GetDefinition(ttGreaterThan)), &t);
			Error(InsteadFound(t), &t);
		}
		else
			isValid = false;
	}
	else
	{
		// Break the token so that only the first > is parsed
		SetPos(t.pos + 1);
	}

	if (!required && !isValid)
	{
		// Restore the original state before returning
		while (node->lastChild != last)
		{
			asCScriptNode *n = node->lastChild;
			n->DisconnectParent();
		}

		return false;
	}

	// The template type list was parsed OK
	return true;
}

asCScriptNode *asCParser::ParseToken(int token)
{
	asCScriptNode *node = CreateNode(snUndefined);
	if( node == 0 ) return 0;

	sToken t1;

	GetToken(&t1);
	if( t1.type != token )
	{
		Error(ExpectedToken(asCTokenizer::GetDefinition(token)), &t1);
		Error(InsteadFound(t1), &t1);
		return node;
	}

	node->SetToken(&t1);
	node->UpdateSourcePos(t1.pos, t1.length);

	return node;
}

asCScriptNode *asCParser::ParseOneOf(int *tokens, int count)
{
	asCScriptNode *node = CreateNode(snUndefined);
	if( node == 0 ) return 0;

	sToken t1;

	GetToken(&t1);
	int n;
	for( n = 0; n < count; n++ )
	{
		if( tokens[n] == t1.type )
			break;
	}
	if( n == count )
	{
		Error(ExpectedOneOf(tokens, count), &t1);
		Error(InsteadFound(t1), &t1);
		return node;
	}

	node->SetToken(&t1);
	node->UpdateSourcePos(t1.pos, t1.length);

	return node;
}

// BNF:5: DATATYPE      ::= (IDENTIFIER | PRIMTYPE | '?' | 'auto')
asCScriptNode *asCParser::ParseDataType(bool allowVariableType, bool allowAuto)
{
	asCScriptNode *node = CreateNode(snDataType);
	if( node == 0 ) return 0;

	sToken t1;

	GetToken(&t1);
	if( !IsDataType(t1) && !(allowVariableType && t1.type == ttQuestion) && !(allowAuto && t1.type == ttAuto) )
	{
		if( t1.type == ttIdentifier )
		{
			asCString errMsg;
			tempString.Assign(&script->code[t1.pos], t1.length);
			errMsg.Format(TXT_IDENTIFIER_s_NOT_DATA_TYPE, tempString.AddressOf());
			Error(errMsg, &t1);
		}
		else if( t1.type == ttAuto )
		{
			Error(TXT_AUTO_NOT_ALLOWED, &t1);
		}
		else
		{
			Error(TXT_EXPECTED_DATA_TYPE, &t1);
			Error(InsteadFound(t1), &t1);
		}
		return node;
	}

	node->SetToken(&t1);
	node->UpdateSourcePos(t1.pos, t1.length);
	ResolveFloatTypeWidth(node, &t1);

	return node;
}

// BNF:6: PRIMTYPE      ::= 'void' | 'int' | 'int8' | 'int16' | 'int32' | 'int64' | 'uint' | 'uint8' | 'uint16' | 'uint32' | 'uint64' | 'float' | 'double' | 'bool'
asCScriptNode *asCParser::ParseRealType()
{
	asCScriptNode *node = CreateNode(snDataType);
	if( node == 0 ) return 0;

	sToken t1;

	GetToken(&t1);
	if( !IsRealType(t1.type) )
	{
		Error(TXT_EXPECTED_DATA_TYPE, &t1);
		Error(InsteadFound(t1), &t1);
		return node;
	}

	node->SetToken(&t1);
	node->UpdateSourcePos(t1.pos, t1.length);
	ResolveFloatTypeWidth(node, &t1);

	return node;
}

void asCParser::ResolveFloatTypeWidth(asCScriptNode* node, sToken* token)
{
	// The way we treat certain real type tokens depends on engine settings
	switch (node->tokenType)
	{
		case ttFloat:
			if (engine->ep.floatIsFloat64)
			{
				node->tokenType = ttFloat64;
			}
			else
			{
				node->tokenType = ttFloat32;
			}

			if (isParsingAppInterface)
			{
				Error("Use of 'float' in manual C++ bindings is ambiguous. Please use 'float32' or 'float64'", token);
			}
		break;
		case ttDouble:
			node->tokenType = ttFloat64;

			if (!engine->ep.allowDoubleType && !isParsingAppInterface)
			{
				if (engine->ep.floatIsFloat64)
					Warning("Use of 'double' is deprecated. Please use 'float' or 'float64'", token);
				else
					Warning("Use of 'double' is deprecated. Please use 'float64'", token);
			}
		break;
	}
}

// BNF:17: IDENTIFIER    ::= single token:  starts with letter or _, can include any letter and digit, same as in C++
asCScriptNode *asCParser::ParseIdentifier()
{
	asCScriptNode *node = CreateNode(snIdentifier);
	if( node == 0 ) return 0;

	sToken t1;

	GetToken(&t1);
	if( t1.type != ttIdentifier )
	{
		Error(TXT_EXPECTED_IDENTIFIER, &t1);
		Error(InsteadFound(t1), &t1);
		return node;
	}

	node->SetToken(&t1);
	node->UpdateSourcePos(t1.pos, t1.length);

	return node;
}

// BNF:3: PARAMLIST     ::= '(' ['void' | (TYPE TYPEMOD [IDENTIFIER] ['=' EXPR] {',' TYPE TYPEMOD [IDENTIFIER] ['=' EXPR]})] ')'
asCScriptNode *asCParser::ParseParameterList(asASTDeclId callableDecl)
{
	asCScriptNode *node = CreateNode(snParameterList);
	if( node == 0 ) return 0;

	sToken t1;
	GetToken(&t1);
	if( t1.type != ttOpenParanthesis )
	{
		Error(ExpectedToken("("), &t1);
		Error(InsteadFound(t1), &t1);
		return node;
	}

	node->UpdateSourcePos(t1.pos, t1.length);

	GetToken(&t1);
	if( t1.type == ttCloseParanthesis )
	{
		node->UpdateSourcePos(t1.pos, t1.length);

		// Statement block is finished
		return node;
	}
	else
	{
		// If the parameter list is just (void) then the void token should be ignored
		if( t1.type == ttVoid )
		{
			sToken t2;
			GetToken(&t2);
			if( t2.type == ttCloseParanthesis )
			{
				node->UpdateSourcePos(t2.pos, t2.length);
				return node;
			}
		}

		RewindTo(&t1);

		for(;;)
		{
			// Parse data type
			asCScriptNode* typeNode = ParseType(true, isParsingAppInterface);
			node->AddChildLast(typeNode);
			if( isSyntaxError ) return node;

			asCScriptNode* typeMod = ParseTypeMod(true);
			node->AddChildLast(typeMod);
			if( isSyntaxError ) return node;

			GetToken(&t1);

			asCScriptNode* nameNode = 0;
			// Application registration declarations may omit parameter names.
			// The grammar has long documented IDENTIFIER as optional, and the
			// maintained public API accepts upstream add-ons that rely on it.
			if( t1.type == ttIdentifier )
			{
				RewindTo(&t1);
				nameNode = ParseIdentifier();
				node->AddChildLast(nameNode);
				if( isSyntaxError ) return node;

				GetToken(&t1);
			}

			// Publish the complete parameter header before parsing its optional
			// default. A malformed default cannot erase name/type/direction facts.
			asASTDeclId canonicalParameter;
			if( sema && callableDecl.IsValid() )
			{
				asSParameterDeclAction action;
				action.callable = callableDecl;
				action.type = sema->ActOnQualTypeAction(
					BuildQualTypeSyntaxAction(typeNode, typeMod), script);
				if( nameNode && script && script->code
					&& nameNode->tokenLength > 0
					&& nameNode->tokenPos + nameNode->tokenLength
						<= script->codeLength )
				{
					action.name.Assign(
						script->code + nameNode->tokenPos,
						(asUINT)nameNode->tokenLength);
				}
				const asCScriptNode* const endNode = nameNode
					? nameNode
					: (typeMod && typeMod->tokenLength > 0 ? typeMod : typeNode);
				if( typeNode && endNode && typeNode->tokenPos >= 0
					&& endNode->tokenPos >= 0 && endNode->tokenLength > 0 )
				{
					action.beginOffset = (asUINT)typeNode->tokenPos;
					action.endOffset = (asUINT)(
						endNode->tokenPos + endNode->tokenLength);
				}
				canonicalParameter = sema->ActOnParameterDeclAction(
					action, script);
			}

			asCScriptNode* defaultNode = 0;
			// Parse optional expression for the default arg
			if( t1.type == ttAssignment )
			{
				if( sema )
				{
					// Canonical Sema owns the default as a typed expression
					// (Clang CXXDefaultArgExpr). Keep the native parameter-list
					// contract as `snExpression`, while retaining the complete
					// parsed assignment beneath that transparent wrapper. Builder
					// still consumes the original syntax-tree shape for Runtime
					// declaration registration and LEGACY/reference routes.
					defaultNode = CreateNode(snExpression);
					if( defaultNode != 0 )
					{
						defaultNode->AddChildLast(ParseAssignment());
						if( defaultNode->firstChild != 0 )
						{
							defaultNode->UpdateSourcePos(
								defaultNode->firstChild->tokenPos,
								defaultNode->firstChild->tokenLength);
						}
					}
				}
				else
				{
					// Legacy still re-parses the default when compiling a call.
					defaultNode = SuperficiallyParseExpression();
				}
				node->AddChildLast(defaultNode);
				if( isSyntaxError ) return node;

				GetToken(&t1);
			}

			if( sema && canonicalParameter.IsValid() && defaultNode != 0 )
			{
				asSParameterDefaultAction action;
				action.parameter = canonicalParameter;
				action.expression =
					FindSingleExactParsedExpressionIdentity(sema, defaultNode);
				action.authoredText = QualTypeActionNodeText(defaultNode, script);
				action.recovered = isSyntaxError;
				BuildCopiedActionRange(
					defaultNode, script, action.beginOffset, action.endOffset);
				sema->ActOnParameterDefaultAction(action, script);
			}

			// Check if list continues
			if( t1.type == ttCloseParanthesis )
			{
				node->UpdateSourcePos(t1.pos, t1.length);
				return node;
			}
			else if( t1.type == ttListSeparator )
			{
				GetToken(&t1);

				// Check if we should close the parameter list or continue
				if( t1.type == ttCloseParanthesis )
				{
					// Parameter list ended with a trailing comma
					node->UpdateSourcePos(t1.pos, t1.length);
					return node;
				}

				RewindTo(&t1);
				continue;
			}
			else
			{
				Error(ExpectedTokens(")", ","), &t1);
				Error(InsteadFound(t1), &t1);
				return node;
			}
		}
	}
	UNREACHABLE_RETURN;
}

asCScriptNode *asCParser::SuperficiallyParseExpression()
{
	asCScriptNode *node = CreateNode(snExpression);
	if( node == 0 ) return 0;

	// Simply parse everything until the first , or ), whichever comes first.
	// Keeping in mind that () and {} can group expressions.

	sToken start;
	GetToken(&start);
	RewindTo(&start);

	asCString stack;
	sToken t;
	for(;;)
	{
		GetToken(&t);

		if( t.type == ttOpenParanthesis )
			stack += "(";
		else if( t.type == ttCloseParanthesis )
		{
			if( stack == "" )
			{
				// Expression has ended. This token is not part of expression
				RewindTo(&t);
				break;
			}
			else if( stack[stack.GetLength()-1] == '(' )
			{
				// Group has ended
				stack.SetLength(stack.GetLength()-1);
			}
			else
			{
				// Wrong syntax
				RewindTo(&t);
				asCString str;
				str.Format(TXT_UNEXPECTED_TOKEN_s, ")");
				Error(str, &t);
				return node;
			}
		}
		else if( t.type == ttListSeparator )
		{
			if( stack == "" )
			{
				// Expression has ended. This token is not part of expression
				RewindTo(&t);
				break;
			}
		}
		else if( t.type == ttStartStatementBlock )
			stack += "{";
		else if( t.type == ttEndStatementBlock )
		{
			if( stack == "" || stack[stack.GetLength()-1] != '{' )
			{
				// Wrong syntax
				RewindTo(&t);
				asCString str;
				str.Format(TXT_UNEXPECTED_TOKEN_s, "}");
				Error(str, &t);
				return node;
			}
			else
			{
				// Group has ended
				stack.SetLength(stack.GetLength()-1);
			}
		}
		else if( t.type == ttEndStatement )
		{
			// Wrong syntax (since we're parsing a default arg expression)
			RewindTo(&t);
			asCString str;
			str.Format(TXT_UNEXPECTED_TOKEN_s, ";");
			Error(str, &t);
			return node;
		}
		else if( t.type == ttNonTerminatedStringConstant )
		{
			RewindTo(&t);
			Error(TXT_NONTERMINATED_STRING, &t);
			return node;
		}
		else if( t.type == ttEnd )
		{
			// Wrong syntax
			RewindTo(&t);
			Error(TXT_UNEXPECTED_END_OF_FILE, &t);
			Info(TXT_WHILE_PARSING_EXPRESSION, &start);
			return node;
		}

		// Include the token in the node
		node->UpdateSourcePos(t.pos, t.length);
	}

	return node;
}

void asCParser::GetToken(sToken *token)
{
	// Check if the token has already been parsed
	if( lastToken.pos == sourcePos )
	{
		*token = lastToken;
		sourcePos += token->length;

		if( token->type == ttWhiteSpace ||
			token->type == ttOnelineComment ||
			token->type == ttMultilineComment )
			GetToken(token);

		return;
	}

	// Parse new token
	size_t sourceLength = script->codeLength;
	do
	{
		if( sourcePos >= sourceLength )
		{
			token->type = ttEnd;
			token->length = 0;
		}
		else
			token->type = engine->tok.GetToken(&script->code[sourcePos], sourceLength - sourcePos, &token->length);

		token->pos = sourcePos;

		// Update state
		sourcePos += token->length;
	}
	// Filter out whitespace and comments
	while( token->type == ttWhiteSpace ||
	       token->type == ttOnelineComment ||
	       token->type == ttMultilineComment );
}

void asCParser::SetPos(size_t pos)
{
	lastToken.pos = size_t(-1);
	sourcePos = pos;
}

void asCParser::RewindTo(const sToken *token)
{
	// TODO: optimize: Perhaps we can optimize this further by having the parser
	//                 set an explicit return point, after which each token will
	//                 be stored. That way not just one token will be reused but
	//                 no token will have to be tokenized more than once.

	// Store the token so it doesn't have to be tokenized again
	lastToken = *token;

	sourcePos = token->pos;
}

void asCParser::Error(const asCString &text, sToken *token)
{
	RewindTo(token);

	isSyntaxError     = true;
	errorWhileParsing = true;

	if( builder )
	{
		int row = 0, col = 0;
		builder->GetDiagnosticRowColumn(script, token->pos, &row, &col);
		builder->WriteError(script->name, text, row, col);
	}
}

void asCParser::Warning(const asCString &text, sToken *token)
{
	if( builder )
	{
		int row = 0, col = 0;
		builder->GetDiagnosticRowColumn(script, token->pos, &row, &col);
		builder->WriteWarning(script->name, text, row, col);
	}
}

void asCParser::Info(const asCString &text, sToken *token)
{
	RewindTo(token);

	isSyntaxError     = true;
	errorWhileParsing = true;

	if( builder )
	{
		int row = 0, col = 0;
		builder->GetDiagnosticRowColumn(script, token->pos, &row, &col);
		builder->WriteInfo(script->name, text, row, col, false);
	}
}

// nextToken is only modified if the current position can be interpreted as
// type, in this case it is set to the next token after the type tokens
bool asCParser::IsType(sToken &nextToken)
{
	// Set a rewind point
	sToken t, t1;
	GetToken(&t);

	// A type can start with a const
	t1 = t;
	if (t1.type == ttConst)
		GetToken(&t1);

	sToken t2;
	if (t1.type != ttAuto)
	{
		// The type may be initiated with the scope operator
		if (t1.type == ttScope)
			GetToken(&t1);

		// The type may be preceeded with a multilevel scope
		GetToken(&t2);
		while (t1.type == ttIdentifier)
		{
			if (t2.type == ttScope)
			{
				GetToken(&t1);
				GetToken(&t2);
				continue;
			}
			else if (t2.type == ttLessThan)
			{
				// Template types can also be used as scope identifiers
				RewindTo(&t2);
				if (CheckTemplateType(t1))
				{
					sToken t3;
					GetToken(&t3);
					if (t3.type == ttScope)
					{
						GetToken(&t1);
						GetToken(&t2);
						continue;
					}
				}
			}

			break;
		}
		RewindTo(&t2);
	}

	// We don't validate if the identifier is an actual declared type at this moment
	// as it may wrongly identify the statement as a non-declaration if the user typed
	// the name incorrectly. The real type is validated in ParseDeclaration where a
	// proper error message can be given.
	if (!IsRealType(t1.type) && t1.type != ttIdentifier && t1.type != ttAuto)
	{
		RewindTo(&t);
		return false;
	}

	if (!CheckTemplateType(t1))
	{
		RewindTo(&t);
		return false;
	}

	// Object handles can be interleaved with the array brackets
	// Even though declaring variables with & is invalid we'll accept
	// it here to give an appropriate error message later
	GetToken(&t2);
	while (t2.type == ttHandle || t2.type == ttAmp || t2.type == ttOpenBracket)
	{
		if (t2.type == ttOpenBracket)
		{
			GetToken(&t2);
			if (t2.type != ttCloseBracket)
			{
				RewindTo(&t);
				return false;
			}
		}

		GetToken(&t2);
	}

	while (t2.type == ttIdentifier && 
		(IdentifierIs(t2, AUTO_CONSTREF_TOKEN)
		|| IdentifierIs(t2, ANY_IMPLICIT_INTEGER_TOKEN)))
	{
		GetToken(&t2);
	}

	// Return the next token so the caller can jump directly to it if desired
	nextToken = t2;

	// Rewind to start point
	RewindTo(&t);

	return true;
}

bool asCParser::IsRealType(int tokenType)
{
	if( tokenType == ttVoid ||
		tokenType == ttInt ||
		tokenType == ttInt8 ||
		tokenType == ttInt16 ||
		tokenType == ttInt64 ||
		tokenType == ttUInt ||
		tokenType == ttUInt8 ||
		tokenType == ttUInt16 ||
		tokenType == ttUInt64 ||
		tokenType == ttFloat32 ||
		tokenType == ttFloat64 ||
		tokenType == ttFloat ||
		tokenType == ttDouble ||
		tokenType == ttBool )
		return true;

	return false;
}

bool asCParser::IsDataType(const sToken &token)
{
	if( token.type == ttIdentifier )
	{
#ifndef AS_NO_COMPILER
		if( checkValidTypes )
		{
			// Check if this is an existing type, regardless of namespace
			tempString.Assign(&script->code[token.pos], token.length);
			if( !DoesTypeExist(tempString.AddressOf()) )
				return false;
		}
#endif
		return true;
	}

	if( IsRealType(token.type) )
		return true;

	return false;
}

bool asCParser::DoesTypeExist(const char* type) const
{
#ifndef AS_NO_COMPILER
	if( builder != 0 )
	{
		return builder->DoesTypeExist(type);
	}
	if( sema != 0 )
	{
		return sema->IsKnownTypeNameForParsing(type);
	}
#else
	UNUSED_VAR(type);
#endif
	return false;
}

asCString asCParser::ExpectedToken(const char *token)
{
	asCString str;

	str.Format(TXT_EXPECTED_s, token);

	return str;
}

asCString asCParser::ExpectedTokens(const char *t1, const char *t2)
{
	asCString str;

	str.Format(TXT_EXPECTED_s_OR_s, t1, t2);

	return str;
}

asCString asCParser::ExpectedOneOf(int *tokens, int count)
{
	asCString str;

	str = TXT_EXPECTED_ONE_OF;
	for( int n = 0; n < count; n++ )
	{
		str += asCTokenizer::GetDefinition(tokens[n]);
		if( n < count-1 )
			str += ", ";
	}

	return str;
}

asCString asCParser::ExpectedOneOf(const char **tokens, int count)
{
	asCString str;

	str = TXT_EXPECTED_ONE_OF;
	for( int n = 0; n < count; n++ )
	{
		str += tokens[n];
		if( n < count-1 )
			str += ", ";
	}

	return str;
}

asCString asCParser::InsteadFound(sToken &t)
{
	asCString str;
	if( t.type == ttIdentifier )
	{
		asCString id(&script->code[t.pos], t.length);
		str.Format(TXT_INSTEAD_FOUND_IDENTIFIER_s, id.AddressOf());
	}
	else if( t.type >= ttIf )
		str.Format(TXT_INSTEAD_FOUND_KEYWORD_s, asCTokenizer::GetDefinition(t.type));
	else
		str.Format(TXT_INSTEAD_FOUND_s, asCTokenizer::GetDefinition(t.type));

	return str;
}

asCScriptNode *asCParser::ParseListPattern()
{
	asCScriptNode *node = CreateNode(snListPattern);
	if( node == 0 ) return 0;

	sToken t1;

	GetToken(&t1);
	if( t1.type != ttStartStatementBlock )
	{
		Error(ExpectedToken("{"), &t1);
		Error(InsteadFound(t1), &t1);
		return node;
	}

	node->UpdateSourcePos(t1.pos, t1.length);

	sToken start = t1;

	bool isBeginning = true;
	bool afterType = false;
	while( !isSyntaxError )
	{
		GetToken(&t1);
		if( t1.type == ttEndStatementBlock )
		{
			if( !afterType )
			{
				Error(TXT_EXPECTED_DATA_TYPE, &t1);
				Error(InsteadFound(t1), &t1);
			}
			break;
		}
		else if( t1.type == ttStartStatementBlock )
		{
			if( afterType )
			{
				Error(ExpectedTokens(",","}"), &t1);
				Error(InsteadFound(t1), &t1);
			}
			RewindTo(&t1);
			node->AddChildLast(ParseListPattern());
			afterType = true;
		}
		else if( t1.type == ttIdentifier && (IdentifierIs(t1, "repeat") || IdentifierIs(t1, "repeat_same")) )
		{
			if( !isBeginning )
			{
				asCString msg;
				asCString token(&script->code[t1.pos], t1.length);
				msg.Format(TXT_UNEXPECTED_TOKEN_s, token.AddressOf());
				Error(msg.AddressOf(), &t1);
			}
			RewindTo(&t1);
			node->AddChildLast(ParseIdentifier());
		}
		else if( t1.type == ttEnd )
		{
			Error(TXT_UNEXPECTED_END_OF_FILE, &t1);
			Info(TXT_WHILE_PARSING_STATEMENT_BLOCK, &start);
			break;
		}
		else if( t1.type == ttListSeparator )
		{
			if( !afterType )
			{
				Error(TXT_EXPECTED_DATA_TYPE, &t1);
				Error(InsteadFound(t1), &t1);
			}
			afterType = false;
		}
		else
		{
			if( afterType )
			{
				Error(ExpectedTokens(",", "}"), &t1);
				Error(InsteadFound(t1), &t1);
			}
			RewindTo(&t1);
			node->AddChildLast(ParseType(true, true));
			afterType = true;
		}

		isBeginning = false;
	}

	node->UpdateSourcePos(t1.pos, t1.length);

	return node;
}

bool asCParser::IdentifierIs(const sToken &t, const char *str)
{
	if( t.type != ttIdentifier )
		return false;

	return script->TokenEquals(t.pos, t.length, str);
}

#ifndef AS_NO_COMPILER

// This function will return true if the current token is not a template, or if it is and
// the following has a valid syntax for a template type. The source position will be left
// at the first token after the type in case of success
bool asCParser::CheckTemplateType(const sToken &t)
{
	// Is this a template type?
	tempString.Assign(&script->code[t.pos], t.length);
	if( engine->IsTemplateType(tempString.AddressOf()) )
	{
		// If the next token is a < then parse the sub-type too
		sToken t1;
		GetToken(&t1);
		if( t1.type != ttLessThan )
		{
			RewindTo(&t1);
			return true;
		}

		for(;;)
		{
			// There might optionally be a 'const'
			GetToken(&t1);
			if( t1.type == ttConst )
				GetToken(&t1);

			// The type may be initiated with the scope operator
			if( t1.type == ttScope )
				GetToken(&t1);

			// There may be multiple levels of scope operators
			sToken t2;
			GetToken(&t2);
			while( t1.type == ttIdentifier && t2.type == ttScope )
			{
				GetToken(&t1);
				GetToken(&t2);
			}
			RewindTo(&t2);

			// Now there must be a data type
			if( !IsDataType(t1) )
				return false;

			if( !CheckTemplateType(t1) )
				return false;

			GetToken(&t1);

			// Is it a handle or array?
			while( t1.type == ttHandleSuffix || t1.type == ttOpenBracket )
			{
				if( t1.type == ttOpenBracket )
				{
					GetToken(&t1);
					if( t1.type != ttCloseBracket )
						return false;
				}

				GetToken(&t1);
			}

			// Was this the last template subtype?
			if( t1.type != ttListSeparator )
				break;
		}

		// Accept >> and >>> tokens too. But then force the tokenizer to move
		// only 1 character ahead (thus splitting the token in two).
		if( script->code[t1.pos] != '>' )
			return false;
		else if( t1.length != 1 )
		{
			// We need to break the token, so that only the first character is parsed
			SetPos(t1.pos + 1);
		}
	}

	return true;
}

// BNF:12: CAST          ::= 'cast' '<' TYPE '>' '(' ASSIGN ')'
asCScriptNode *asCParser::ParseCast()
{
	asCScriptNode *node = CreateNode(snCast);
	if( node == 0 ) return 0;

	sToken t1;
	GetToken(&t1);
	if( t1.type != ttCast )
	{
		Error(ExpectedToken("cast"), &t1);
		Error(InsteadFound(t1), &t1);
		return node;
	}

	node->UpdateSourcePos(t1.pos, t1.length);

	GetToken(&t1);
	if( t1.type != ttLessThan )
	{
		Error(ExpectedToken("<"), &t1);
		Error(InsteadFound(t1), &t1);
		return node;
	}

	// Parse and resolve the target while Parser still owns the complete type
	// syntax. Bind it only after the recoverable expression range is final so
	// the pointer-free action identity cannot change beneath the binding.
	asCScriptNode* const typeNode = ParseType(true);
	node->AddChildLast(typeNode);
	asCQualType targetType;
	if( sema && typeNode )
	{
		targetType = sema->ActOnQualTypeAction(
			BuildQualTypeSyntaxAction(typeNode, 0), script);
	}
	if( isSyntaxError ) return node;

	GetToken(&t1);
	if( t1.type != ttGreaterThan )
	{
		Error(ExpectedToken(">"), &t1);
		Error(InsteadFound(t1), &t1);
		return node;
	}

	GetToken(&t1);
	if( t1.type != ttOpenParanthesis )
	{
		Error(ExpectedToken("("), &t1);
		Error(InsteadFound(t1), &t1);
		return node;
	}

	node->AddChildLast(ParseCondition());
	if( isSyntaxError )
	{
		// A nested expression can publish its exact identity before reporting a
		// later delimiter error. Bind the cast only when both that operand and
		// the Parser-resolved target type are already complete.
		BindCastExprAction(node, script, sema, targetType);
		return node;
	}

	GetToken(&t1);
	if( t1.type != ttCloseParanthesis )
	{
		Error(ExpectedToken(")"), &t1);
		Error(InsteadFound(t1), &t1);
		BindCastExprAction(node, script, sema, targetType);
		return node;
	}

	node->UpdateSourcePos(t1.pos, t1.length);
	BindCastExprAction(node, script, sema, targetType);

	return node;
}

// BNF:11: EXPRVALUE     ::= 'void' | CONSTRUCTCALL | FUNCCALL | VARACCESS | CAST | LITERAL | '(' ASSIGN ')' | LAMBDA
asCScriptNode *asCParser::ParseExprValue()
{
	asCScriptNode *node = CreateNode(snExprValue);
	if( node == 0 ) return 0;

	sToken t1, t2;
	GetToken(&t1);
	GetToken(&t2);
	RewindTo(&t1);

	// 'void' is a special expression that doesn't do anything (normally used for skipping output arguments)
	if( t1.type == ttVoid )
		node->AddChildLast(ParseToken(ttVoid));
	else if( IsRealType(t1.type) )
		node->AddChildLast(ParseConstructCall());
	else if( t1.type == ttIdentifier || t1.type == ttScope )
	{
		// Check if the expression is an anonymous function
		if( IsLambda() )
		{
			node->AddChildLast(ParseLambda());
		}
		else
		{
			// Determine the last identifier in order to check if it is a type
			sToken t;
			if( t1.type == ttScope ) t = t2; else t = t1;
			RewindTo(&t);
			GetToken(&t2);
			while( t.type == ttIdentifier )
			{
				t2 = t;
				GetToken(&t);
				if( t.type == ttScope )
					GetToken(&t);
				else
					break;
			}

			bool isDataType = IsDataType(t2);
			bool isTemplateType = false;
			if( isDataType )
			{
				// Is this a template type?
				tempString.Assign(&script->code[t2.pos], t2.length);
				if( engine->IsTemplateType(tempString.AddressOf()) )
					isTemplateType = true;
			}

			GetToken(&t2);

			// Rewind so the real parsing can be done, after deciding what to parse
			RewindTo(&t1);

			// Check if this is a construct call
			// Just 'type()' isn't considered a construct call, because type may just be a function/method name.
			// The compiler will have to sort this out, since the parser doesn't have enough information.
			if( isDataType && (t.type == ttOpenBracket && t2.type == ttCloseBracket) )      // type[]()
				node->AddChildLast(ParseConstructCall());
			else if( isTemplateType && t.type == ttLessThan )  // type<t>()
				node->AddChildLast(ParseConstructCall());
			else if( IsFunctionCall() )
				node->AddChildLast(ParseFunctionCall());
			else
				node->AddChildLast(ParseVariableAccess());
		}
	}
	else if( t1.type == ttCast )
		node->AddChildLast(ParseCast());
	else if( IsConstant(t1.type) )
		node->AddChildLast(ParseConstant());
	else if( t1.type == ttOpenParanthesis )
	{
		GetToken(&t1);
		node->UpdateSourcePos(t1.pos, t1.length);

		node->AddChildLast(ParseCondition());
		if( isSyntaxError ) return node;

		GetToken(&t1);
		if( t1.type != ttCloseParanthesis )
		{
			Error(ExpectedToken(")"), &t1);
			Error(InsteadFound(t1), &t1);
		}

		node->UpdateSourcePos(t1.pos, t1.length);
	}
	else
	{
		Error(TXT_EXPECTED_EXPRESSION_VALUE, &t1);
		Error(InsteadFound(t1), &t1);
	}

	return node;
}

// BNF:12: LITERAL       ::= NUMBER | STRING | BITS | 'true' | 'false' | 'null'
// BNF:17: NUMBER        ::= single token:  includes integers and real numbers, same as C++
// BNF:17: STRING        ::= single token:  single quoted ', double quoted ", or heredoc multi-line string """
// BNF:17: BITS          ::= single token:  binary 0b or 0B, octal 0o or 0O, decimal 0d or 0D, hexadecimal 0x or 0X
asCScriptNode *asCParser::ParseConstant()
{
	asCScriptNode *node = CreateNode(snConstant);
	if( node == 0 ) return 0;

	sToken t;
	GetToken(&t);
	if( !IsConstant(t.type) )
	{
		Error(TXT_EXPECTED_CONSTANT, &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->SetToken(&t);
	node->UpdateSourcePos(t.pos, t.length);

	// We want to gather a list of string constants to concatenate as children
	if( t.type == ttStringConstant || t.type == ttMultilineStringConstant || t.type == ttHeredocStringConstant )
	{
		RewindTo(&t);
		node->AddChildLast(ParseStringConstant());
	}

	if( sema && script && script->code && t.pos >= 0 && t.length > 0
		&& t.pos + t.length <= script->codeLength )
	{
		asSLiteralExprAction action;
		action.token = t.type;
		action.spelling.Assign(script->code + t.pos, (asUINT)t.length);
		action.beginOffset = (asUINT)t.pos;
		action.endOffset = (asUINT)(t.pos + t.length);
		const asASTExprId expression =
			sema->ActOnLiteralExprAction(action, script);
		if( expression.IsValid() )
		{
			sema->BindParsedExpressionIdentity(
				MakeParseActionIdentity(sema, node), expression);
		}
	}

	return node;
}

bool asCParser::IsLambda()
{
	bool isLambda = false;
	sToken t;
	GetToken(&t);
	if( t.type == ttIdentifier && IdentifierIs(t, FUNCTION_TOKEN) )
	{
		sToken t2;
		GetToken(&t2);
		if( t2.type == ttOpenParanthesis )
		{
			// Skip until )
			while( t2.type != ttCloseParanthesis && t2.type != ttEnd )
				GetToken(&t2);

			// The next token must be a {
			GetToken(&t2);
			if( t2.type == ttStartStatementBlock )
				isLambda = true;
		}
	}

	RewindTo(&t);
	return isLambda;
}

// BNF:12: LAMBDA        ::= 'function' '(' [[TYPE TYPEMOD] IDENTIFIER {',' [TYPE TYPEMOD] IDENTIFIER}] ')' STATBLOCK
asCScriptNode *asCParser::ParseLambda()
{
	asCScriptNode *node = CreateNode(snFunction);
	if( node == 0 ) return 0;
	asCArray<asSParameterDeclAction> canonicalParameters;
	asCQualType recoveryReturnType;
	asUINT canonicalParameterCount = 0;
	asUINT explicitlyTypedParameterCount = 0;

	sToken t;
	GetToken(&t);

	if( t.type != ttIdentifier || !IdentifierIs(t, FUNCTION_TOKEN) )
	{
		Error(ExpectedToken("function"), &t);
		return node;
	}

	asCScriptNode *functionIdent = CreateNode(snIdentifier);
	if( functionIdent == 0 ) return node;
	functionIdent->SetToken(&t);
	functionIdent->UpdateSourcePos(t.pos, t.length);
	node->AddChildLast(functionIdent);

	asCScriptNode *params = CreateNode(snParameterList);
	if( params == 0 ) return node;
	node->AddChildLast(params);

	GetToken(&t);
	if( t.type != ttOpenParanthesis )
	{
		Error(ExpectedToken("("), &t);
		return node;
	}
	params->UpdateSourcePos(t.pos, t.length);

	asCScriptNode* parameterType = 0;
	asCScriptNode* parameterTypeMod = 0;
	// Parse optional type before parameter name
	if( IsType(t) && (t.type == ttAmp || t.type == ttIdentifier) )
	{
		parameterType = ParseType(true);
		params->AddChildLast(parameterType);
		if (isSyntaxError) return node;
		parameterTypeMod = ParseTypeMod(true);
		params->AddChildLast(parameterTypeMod);
		if (isSyntaxError) return node;
	}

	GetToken(&t);
	if( t.type == ttIdentifier )
	{
		RewindTo(&t);
		asCScriptNode* parameterName = ParseIdentifier();
		params->AddChildLast(parameterName);
		if (isSyntaxError) return node;
		++canonicalParameterCount;
		if( parameterType != 0 )
		{
			++explicitlyTypedParameterCount;
		}
		BuildLambdaParameterAction(
			parameterType, parameterTypeMod, parameterName,
			canonicalParameters, recoveryReturnType);

		GetToken(&t);
		while( t.type == ttListSeparator )
		{
			parameterType = 0;
			parameterTypeMod = 0;
			// Parse optional type before parameter name
			if (IsType(t) && (t.type == ttAmp || t.type == ttIdentifier)) 
			{
				parameterType = ParseType(true);
				params->AddChildLast(parameterType);
				if (isSyntaxError) return node;
				parameterTypeMod = ParseTypeMod(true);
				params->AddChildLast(parameterTypeMod);
				if (isSyntaxError) return node;
			}

			parameterName = ParseIdentifier();
			params->AddChildLast(parameterName);
			if( isSyntaxError ) return node;
			++canonicalParameterCount;
			if( parameterType != 0 )
			{
				++explicitlyTypedParameterCount;
			}
			BuildLambdaParameterAction(
				parameterType, parameterTypeMod, parameterName,
				canonicalParameters, recoveryReturnType);

			GetToken(&t);
		}
	}

	if( t.type != ttCloseParanthesis )
	{
		Error(ExpectedToken(")"), &t);
		return node;
	}
	params->UpdateSourcePos(t.pos, t.length);

	asASTDeclId canonicalLambda;
	asSDeclContextScope lambdaScope(sema);
	if( sema )
	{
		if( !recoveryReturnType.IsValid() )
		{
			recoveryReturnType = sema->GetContext().InternPrimitive(ttInt, 0);
		}
		asSLambdaHeaderAction action;
		action.owner = sema->CurrentDeclContext();
		action.recoveryReturnType = recoveryReturnType;
		action.parameterCount = canonicalParameterCount;
		action.explicitlyTypedParameterCount =
			explicitlyTypedParameterCount;
		action.beginOffset = (asUINT)functionIdent->tokenPos;
		action.endOffset = (asUINT)(t.pos + t.length);
		canonicalLambda = sema->ActOnLambdaHeaderAction(action, script);
		for( asUINT i = 0; i < canonicalParameters.GetLength(); ++i )
		{
			canonicalParameters[i].callable = canonicalLambda;
			sema->ActOnParameterDeclAction(canonicalParameters[i], script);
		}
		sema->FinishDecl(canonicalLambda);
		sema->BindParsedDeclarationIdentity(
			MakeParseActionIdentity(sema, node), canonicalLambda);
		lambdaScope.Push(canonicalLambda);
	}

	// Canonical Sema needs a real statement tree; the compiler still re-parses
	// from the block span when the legacy pipeline is selected.
	asCScriptNode* const body = ParseFunctionStatementBlock();
	node->AddChildLast(body);
	if( sema && canonicalLambda.IsValid() && body != 0 && !isSyntaxError )
	{
		asSFunctionBodyAction bodyAction;
		bodyAction.function = canonicalLambda;
		bodyAction.beginOffset = (asUINT)body->tokenPos;
		bodyAction.endOffset =
			(asUINT)(body->tokenPos + body->tokenLength);
		bodyAction.recovered = isSyntaxError;
		bodyAction.body = FindExactParsedStatementIdentity(body);
		sema->ActOnFunctionBodyAction(bodyAction, script);
		asSLambdaExpressionAction expressionAction;
		expressionAction.lambdaDecl = canonicalLambda;
		expressionAction.beginOffset = (asUINT)node->tokenPos;
		expressionAction.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
		const asASTExprId expression = sema->ActOnLambdaExpressionAction(
			expressionAction, script);
		sema->BindParsedExpressionIdentity(
			MakeParseActionIdentity(sema, node), expression);
	}
	BindCompletedParsedDeclarationIdentity(sema, node, canonicalLambda);

	return node;
}

asCScriptNode *asCParser::ParseStringConstant()
{
	asCScriptNode *node = CreateNode(snConstant);
	if( node == 0 ) return 0;

	sToken t;
	GetToken(&t);
	if( t.type != ttStringConstant && t.type != ttMultilineStringConstant && t.type != ttHeredocStringConstant )
	{
		Error(TXT_EXPECTED_STRING, &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->SetToken(&t);
	node->UpdateSourcePos(t.pos, t.length);

	return node;
}

// BNF:12: FUNCCALL      ::= SCOPE IDENTIFIER ARGLIST
asCScriptNode *asCParser::ParseFunctionCall(bool notifySema)
{
	asCScriptNode *node = CreateNode(snFunctionCall);
	if( node == 0 ) return 0;

	// Parse scope prefix
	ParseOptionalScope(node);

	// Parse the function name followed by the argument list
	asCScriptNode* ident = ParseIdentifier();
	node->AddChildLast(ident);
	if( ident )
	{
		node->UpdateSourcePos(ident->tokenPos, ident->tokenLength);
	}
	if( isSyntaxError ) return node;

	node->AddChildLast(ParseArgList());
	// Member postfix '.' supplies the receiver through the ordered structural
	// postfix typed action after the complete receiver chain is available.
	// ActOn here would intern a receiver-less CALL beside the real member CALL.
	if( sema && notifySema )
	{
		BindCallExprAction(node, script, sema, asASTExprId(), isSyntaxError);
	}

	return node;
}

// BNF:12: VARACCESS     ::= SCOPE IDENTIFIER
asCScriptNode *asCParser::ParseVariableAccess()
{
	asCScriptNode *node = CreateNode(snVariableAccess);
	if( node == 0 ) return 0;

	// Parse scope prefix
	ParseOptionalScope(node);

	// Parse the variable name
	asCScriptNode* const identifier = ParseIdentifier();
	node->AddChildLast(identifier);
	if( sema )
	{
		asSDeclRefExprAction action;
		if( BuildDeclRefExprAction(
			node, script, sema->CurrentDeclContext(), action) )
		{
			const asASTExprId expression =
				sema->ActOnDeclRefExprAction(action, script);
			if( expression.IsValid() )
			{
				sema->BindParsedExpressionIdentity(
					MakeParseActionIdentity(sema, node), expression);
			}
		}
	}

	return node;
}

// BNF:11: CONSTRUCTCALL ::= TYPE ARGLIST
asCScriptNode *asCParser::ParseConstructCall()
{
	asCScriptNode *node = CreateNode(snConstructCall);
	if( node == 0 ) return 0;

	asCScriptNode* const typeNode = ParseType(false);
	node->AddChildLast(typeNode);
	asCQualType targetType;
	if( sema && typeNode )
	{
		targetType = sema->ActOnQualTypeAction(
			BuildQualTypeSyntaxAction(typeNode, 0), script);
	}
	if( isSyntaxError ) return node;

	node->AddChildLast(ParseArgList());
	BindConstructExprAction(node, script, sema, targetType);

	return node;
}

// BNF:13: ARGLIST       ::= '(' [IDENTIFIER ':'] ASSIGN {',' [IDENTIFIER ':'] ASSIGN} ')'
asCScriptNode *asCParser::ParseArgList(bool withParenthesis)
{
	asCScriptNode *node = CreateNode(snArgList);
	if( node == 0 ) return 0;

	sToken t1;
	if( withParenthesis )
	{
		GetToken(&t1);
		if( t1.type != ttOpenParanthesis )
		{
			Error(ExpectedToken("("), &t1);
			Error(InsteadFound(t1), &t1);
			return node;
		}

		node->UpdateSourcePos(t1.pos, t1.length);
	}

	GetToken(&t1);
	if( t1.type == ttCloseParanthesis || t1.type == ttCloseBracket )
	{
		if( withParenthesis )
		{
			if( t1.type == ttCloseParanthesis )
				node->UpdateSourcePos(t1.pos, t1.length);
			else
			{
				asCString str;
				str.Format(TXT_UNEXPECTED_TOKEN_s, asCTokenizer::GetDefinition(ttCloseBracket));

				Error(str.AddressOf(), &t1);
			}
		}
		else
			RewindTo(&t1);

		// Argument list has ended
		return node;
	}
	else
	{
		RewindTo(&t1);

		for(;;)
		{
			// Determine if this is a named argument
			sToken tl, t2;
			GetToken(&tl);
			GetToken(&t2);
			RewindTo(&tl);

			// We might have a close parenthesis right away, if it ended with a trailing comma
			if (tl.type == ttCloseParanthesis)
			{
				GetToken(&t1);
				node->UpdateSourcePos(t1.pos, t1.length);
				return node;
			}

			// Named arguments uses the syntax: arg : expr
			// This avoids confusion when the argument has the same name as a local variable, i.e. var = expr
			// It also avoids conflict with expressions to that creates anonymous objects initialized with lists, i.e. type = {...}
			// The alternate syntax: arg = expr, is supported to provide backwards compatibility with 2.29.0
			// TODO: 3.0.0: Remove the alternate syntax
			if( tl.type == ttIdentifier && (t2.type == ttColon || (engine->ep.alterSyntaxNamedArgs && t2.type == ttAssignment)) )
			{
				asCScriptNode *named = CreateNode(snNamedArgument);
				if( named == 0 ) return 0;
				node->AddChildLast(named);

				named->AddChildLast(ParseIdentifier());
				GetToken(&t2);

				//if( engine->ep.alterSyntaxNamedArgs == 1 && t2.type == ttAssignment )
				//	Warning(TXT_NAMED_ARGS_WITH_OLD_SYNTAX, &t2);

				named->AddChildLast(ParseCondition());
			}
			else
				node->AddChildLast(ParseCondition());

			if( isSyntaxError ) return node;

			// Check if list continues
			GetToken(&t1);
			if( t1.type == ttListSeparator )
				continue;
			else
			{
				if( withParenthesis )
				{
					if( t1.type == ttCloseParanthesis )
						node->UpdateSourcePos(t1.pos, t1.length);
					else
					{
						Error(ExpectedTokens(")", ","), &t1);
						Error(InsteadFound(t1), &t1);
					}
				}
				else
					RewindTo(&t1);

				return node;
			}
		}
	}
}

bool asCParser::IsFunctionCall()
{
	sToken s;
	sToken t1, t2;

	GetToken(&s);
	t1 = s;

	// A function call may be prefixed with scope resolution
	if( t1.type == ttScope )
		GetToken(&t1);
	GetToken(&t2);

	while( t1.type == ttIdentifier && t2.type == ttScope )
	{
		GetToken(&t1);
		GetToken(&t2);
	}

	// A function call starts with an identifier followed by an argument list
	// The parser doesn't have enough information about scope to determine if the
	// identifier is a datatype, so even if it happens to be the parser will
	// identify the expression as a function call rather than a construct call.
	// The compiler will sort this out later
	if( t1.type != ttIdentifier )
	{
		RewindTo(&s);
		return false;
	}

	if( t2.type == ttOpenParanthesis )
	{
		RewindTo(&s);
		return true;
	}

	RewindTo(&s);
	return false;
}

// BNF:13: ASSIGN        ::= CONDITION [ ASSIGNOP ASSIGN ]
asCScriptNode *asCParser::ParseAssignment()
{
	asCScriptNode *node = CreateNode(snAssignment);
	if( node == 0 ) return 0;

	node->AddChildLast(ParseCondition());
	if( isSyntaxError ) return node;

	sToken t;
	GetToken(&t);
	RewindTo(&t);

	if( IsAssignOperator(t.type) )
	{
		node->AddChildLast(ParseAssignOperator());
		if( isSyntaxError ) return node;

		node->AddChildLast(ParseAssignment());
		if( isSyntaxError )
		{
			// A later delimiter error may occur after the recursively parsed RHS
			// has already published its exact identity. Publish this assignment
			// whenever both exact operands and the operator are complete.
			BindAssignExprAction(node, script, sema);
			return node;
		}
		BindAssignExprAction(node, script, sema);
	}

	return node;
}

// BNF:14: CONDITION     ::= EXPR ['?' ASSIGN ':' ASSIGN]
asCScriptNode *asCParser::ParseCondition()
{
	asCScriptNode *node = CreateNode(snCondition);
	if( node == 0 ) return 0;

	node->AddChildLast(ParseExpression());
	if( isSyntaxError ) return node;
	if( node->firstChild )
	{
		node->UpdateSourcePos(node->firstChild->tokenPos, node->firstChild->tokenLength);
	}

	sToken t;
	GetToken(&t);
	if( t.type == ttQuestion )
	{
		node->AddChildLast(ParseCondition());
		if( isSyntaxError )
		{
			return node;
		}

		GetToken(&t);
		if( t.type != ttColon )
		{
			Error(ExpectedToken(":"), &t);
			Error(InsteadFound(t), &t);
			return node;
		}

		node->AddChildLast(ParseCondition());
		if( isSyntaxError )
		{
			// A nested operand may report a later delimiter error after already
			// publishing its exact expression identity (for example `G(2` with a
			// missing call parenthesis). If all three semantic operands exist, the
			// ternary grammar itself is complete enough to publish for recovery.
			BindConditionalExprAction(node, script, sema);
			return node;
		}
		if( sema )
		{
			BindConditionalExprAction(node, script, sema);
		}
	}
	else
		RewindTo(&t);

	return node;
}

asCScriptNode *asCParser::ParseConditionAsAssignment()
{
	asCScriptNode *node = CreateNode(snAssignment);
	if( node == 0 ) return 0;

	node->AddChildLast(ParseCondition());
	return node;
}

// BNF:9: EXPR          ::= EXPRTERM {EXPROP EXPRTERM}
asCScriptNode *asCParser::ParseExpression()
{
	asCScriptNode *node = CreateNode(snExpression);
	if( node == 0 ) return 0;

	node->AddChildLast(ParseExprTerm());
	if( isSyntaxError ) return node;
	if( node->firstChild )
	{
		node->UpdateSourcePos(node->firstChild->tokenPos, node->firstChild->tokenLength);
	}

	for(;;)
	{
		sToken t;
		GetToken(&t);
		RewindTo(&t);

		if( !IsOperator(t.type) )
		{
			BindBinaryExprAction(node, script, sema);
			return node;
		}

		node->AddChildLast(ParseExprOperator());
		if( isSyntaxError ) return node;

		node->AddChildLast(ParseExprTerm());
		if( isSyntaxError )
		{
			// A later grammar error may be reported after the last complete term
			// already published its exact identity. Publish only an alternating,
			// complete operand/operator sequence; the builder fails closed for a
			// missing term or trailing operator.
			BindBinaryExprAction(node, script, sema);
			return node;
		}
	}
	UNREACHABLE_RETURN;
}

// BNF:10: EXPRTERM      ::= ([TYPE '='] INITLIST) | ({EXPRPREOP} EXPRVALUE {EXPRPOSTOP})
asCScriptNode *asCParser::ParseExprTerm()
{
	asCScriptNode *node = CreateNode(snExprTerm);
	if( node == 0 ) return 0;

	// Check if the expression term is an initialization of a temp object with init list, i.e. type = {...}
	sToken t;
	GetToken(&t);
	sToken t2 = t, t3;
	if (IsDataType(t2) && CheckTemplateType(t2))
	{
		// The next token must be a = followed by a {
		GetToken(&t2);
		GetToken(&t3);
		if (t2.type == ttAssignment && t3.type == ttStartStatementBlock)
		{
			// It is an initialization, now parse it for real
			RewindTo(&t);
			asCScriptNode* const typeNode = ParseType(false);
			node->AddChildLast(typeNode);
			asCQualType targetType;
			if( sema && typeNode )
			{
				targetType = sema->ActOnQualTypeAction(
					BuildQualTypeSyntaxAction(typeNode, 0), script);
			}
			GetToken(&t2);
			asCScriptNode* const initList = ParseInitList();
			node->AddChildLast(initList);
			BindInitListExprAction(
				node, script, sema, initList, targetType, true,
				isSyntaxError);
			return node;
		}
	}
	// Or an anonymous init list, i.e. {...}
	else if( t.type == ttStartStatementBlock )
	{
		RewindTo(&t);
		asCScriptNode* const initList = ParseInitList();
		node->AddChildLast(initList);
		BindInitListExprAction(
			node, script, sema, initList, asCQualType(), false,
			isSyntaxError);
		return node;
	}

	// It wasn't an initialization, so it must be an ordinary expression term
	RewindTo(&t);

	for(;;)
	{
		GetToken(&t);
		RewindTo(&t);
		if( !IsPreOperator(t.type) )
			break;

		node->AddChildLast(ParseExprPreOp());
		if( isSyntaxError ) return node;
	}

	node->AddChildLast(ParseExprValue());
	if( isSyntaxError )
	{
		if( ClassifyExprTermSemanticRoute(node)
			== asEXPR_TERM_SEMANTIC_PURE_UNARY )
		{
			// Publish only if the malformed primary still supplied one exact
			// identity. BuildUnaryExprAction fails closed otherwise; native syntax
			// recovery remains independently available to LEGACY.
			BindUnaryExprAction(node, script, sema);
		}
		return node;
	}
	if( node->lastChild )
	{
		node->UpdateSourcePos(node->lastChild->tokenPos, node->lastChild->tokenLength);
	}

	for(;;)
	{
		GetToken(&t);
		RewindTo(&t);
		if( !IsPostOperator(t.type) )
		{
			const asEExprTermSemanticRoute route =
				ClassifyExprTermSemanticRoute(node);
			if( route == asEXPR_TERM_SEMANTIC_PURE_UNARY )
			{
				BindUnaryExprAction(node, script, sema);
			}
			else if( route == asEXPR_TERM_SEMANTIC_STRUCTURAL_POSTFIX
				&& sema )
			{
				BindStructuralPostfixExprAction(node, script, sema);
			}
			return node;
		}

		node->AddChildLast(ParseExprPostOp());
		if( isSyntaxError )
		{
			const asEExprTermSemanticRoute route =
				ClassifyExprTermSemanticRoute(node);
			if( route == asEXPR_TERM_SEMANTIC_PURE_UNARY )
			{
				BindUnaryExprAction(node, script, sema);
			}
			else if( route == asEXPR_TERM_SEMANTIC_STRUCTURAL_POSTFIX
				&& sema )
			{
				BindStructuralPostfixExprAction(
					node, script, sema, true);
			}
			return node;
		}
	}
	UNREACHABLE_RETURN;
}

// BNF:11: EXPRPREOP     ::= '-' | '+' | '!' | '++' | '--' | '~' | '@'
asCScriptNode *asCParser::ParseExprPreOp()
{
	asCScriptNode *node = CreateNode(snExprPreOp);
	if( node == 0 ) return 0;

	sToken t;
	GetToken(&t);
	if( !IsPreOperator(t.type) )
	{
		Error(TXT_EXPECTED_PRE_OPERATOR, &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->SetToken(&t);
	node->UpdateSourcePos(t.pos, t.length);

	return node;
}

// BNF:11: EXPRPOSTOP    ::= ('.' (FUNCCALL | IDENTIFIER)) | ('[' [IDENTIFIER ':'] ASSIGN {',' [IDENTIFIER ':' ASSIGN} ']') | ARGLIST | '++' | '--'
asCScriptNode *asCParser::ParseExprPostOp()
{
	asCScriptNode *node = CreateNode(snExprPostOp);
	if( node == 0 ) return 0;

	sToken t;
	GetToken(&t);
	if( !IsPostOperator(t.type) )
	{
		Error(TXT_EXPECTED_POST_OPERATOR, &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->SetToken(&t);
	node->UpdateSourcePos(t.pos, t.length);

	if( t.type == ttDot )
	{
		sToken t1, t2;
		GetToken(&t1);
		GetToken(&t2);
		RewindTo(&t1);
		if( t2.type == ttOpenParanthesis )
			node->AddChildLast(ParseFunctionCall(false));
		else
			node->AddChildLast(ParseIdentifier());
	}
	else if( t.type == ttOpenBracket )
	{
		node->AddChildLast(ParseArgList(false));

		GetToken(&t);
		if( t.type != ttCloseBracket )
		{
			Error(ExpectedToken("]"), &t);
			Error(InsteadFound(t), &t);
			return node;
		}

		node->UpdateSourcePos(t.pos, t.length);
	}
	else if( t.type == ttOpenParanthesis )
	{
		RewindTo(&t);
		node->AddChildLast(ParseArgList());
	}

	return node;
}

// BNF:15: EXPROP        ::= MATHOP | COMPOP | LOGICOP | BITOP
// BNF:16: MATHOP        ::= '+' | '-' | '*' | '/' | '%' | '**'
// BNF:16: COMPOP        ::= '==' | '!=' | '<' | '<=' | '>' | '>=' | 'is' | '!is'
// BNF:16: LOGICOP       ::= '&&' | '||' | '^^' | 'and' | 'or' | 'xor'
// BNF:16: BITOP         ::= '&' | '|' | '^' | '<<' | '>>' | '>>>'
asCScriptNode *asCParser::ParseExprOperator()
{
	asCScriptNode *node = CreateNode(snExprOperator);
	if( node == 0 ) return 0;

	sToken t;
	GetToken(&t);
	if( !IsOperator(t.type) )
	{
		Error(TXT_EXPECTED_OPERATOR, &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->SetToken(&t);
	node->UpdateSourcePos(t.pos, t.length);

	return node;
}

// BNF:16: ASSIGNOP      ::= '=' | '+=' | '-=' | '*=' | '/=' | '|=' | '&=' | '^=' | '%=' | '**=' | '<<=' | '>>=' | '>>>='
asCScriptNode *asCParser::ParseAssignOperator()
{
	asCScriptNode *node = CreateNode(snExprOperator);
	if( node == 0 ) return 0;

	sToken t;
	GetToken(&t);
	if( !IsAssignOperator(t.type) )
	{
		Error(TXT_EXPECTED_OPERATOR, &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->SetToken(&t);
	node->UpdateSourcePos(t.pos, t.length);

	return node;
}

bool asCParser::IsOperator(int tokenType)
{
	if( tokenType == ttPlus ||
		tokenType == ttMinus ||
		tokenType == ttStar ||
		tokenType == ttSlash ||
		tokenType == ttPercent ||
		tokenType == ttStarStar ||
		tokenType == ttAnd ||
		tokenType == ttOr ||
		tokenType == ttXor ||
		tokenType == ttEqual ||
		tokenType == ttNotEqual ||
		tokenType == ttLessThan ||
		tokenType == ttLessThanOrEqual ||
		tokenType == ttGreaterThan ||
		tokenType == ttGreaterThanOrEqual ||
		tokenType == ttAmp ||
		tokenType == ttBitOr ||
		tokenType == ttBitXor ||
		tokenType == ttBitShiftLeft ||
		tokenType == ttBitShiftRight ||
		tokenType == ttBitShiftRightArith ||
		tokenType == ttIs ||
		tokenType == ttNotIs )
		return true;

	return false;
}

bool asCParser::IsAssignOperator(int tokenType)
{
	if( tokenType == ttAssignment ||
		tokenType == ttAddAssign ||
		tokenType == ttSubAssign ||
		tokenType == ttMulAssign ||
		tokenType == ttDivAssign ||
		tokenType == ttModAssign ||
		tokenType == ttPowAssign ||
		tokenType == ttAndAssign ||
		tokenType == ttOrAssign ||
		tokenType == ttXorAssign ||
		tokenType == ttShiftLeftAssign ||
		tokenType == ttShiftRightLAssign ||
		tokenType == ttShiftRightAAssign )
		return true;

	return false;
}

bool asCParser::IsPreOperator(int tokenType)
{
	if( tokenType == ttMinus ||
		tokenType == ttPlus ||
		tokenType == ttNot ||
		tokenType == ttInc ||
		tokenType == ttDec ||
		tokenType == ttBitNot ||
		tokenType == ttHandle )
		return true;
	return false;
}

bool asCParser::IsPostOperator(int tokenType)
{
	if( tokenType == ttInc ||            // post increment
		tokenType == ttDec ||            // post decrement
		tokenType == ttDot ||            // member access
		tokenType == ttOpenBracket ||    // index operator
		tokenType == ttOpenParanthesis ) // argument list for call on function pointer
		return true;
	return false;
}

bool asCParser::IsConstant(int tokenType)
{
	if( tokenType == ttIntConstant ||
		tokenType == ttFloat32Constant ||
		tokenType == ttFloat64Constant ||
		tokenType == ttStringConstant ||
		tokenType == ttMultilineStringConstant ||
		tokenType == ttHeredocStringConstant ||
		tokenType == ttTrue ||
		tokenType == ttFalse ||
		tokenType == ttBitsConstant ||
		tokenType == ttNull )
		return true;

	return false;
}

int asCParser::ParseScript(asCScriptCode *in_script)
{
	Reset();

	this->script = in_script;

	scriptNode = ParseScript(false);

	if( errorWhileParsing )
		return -1;

	if( sema && scriptNode && script )
	{
		sema->ResolveDeferredNames();
		sema->ValidateCapturedLambdaUses();
		sema->ValidateDefaultArgumentOverloads();
		sema->ValidateEditorOnlyOverrides();
	}

	// TODO: Should allow application to request this warning to be generated.
	//       It should be off by default, since pre-processor may remove all
	//       code from a section while still being meant as valid code
/*
	// Warn in case there isn't anything in the script
	if( scriptNode->firstChild == 0 )
	{
		if( builder )
			builder->WriteWarning(script->name, TXT_SECTION_IS_EMPTY, 1, 1);
	}
*/
	return 0;
}

int asCParser::ParseExpression(asCScriptCode *in_script)
{
	Reset();

	this->script = in_script;

	checkValidTypes = true;

	scriptNode = ParseExpression();
	if( errorWhileParsing )
		return -1;

	return 0;
}

// BNF:1: IMPORT        ::= 'import' TYPE ['&'] IDENTIFIER PARAMLIST 'from' STRING ';'
asCScriptNode *asCParser::ParseImport()
{
	asCScriptNode *node = CreateNode(snImport);
	if( node == 0 ) return 0;

	sToken t;
	GetToken(&t);
	if( t.type != ttImport )
	{
		Error(ExpectedToken(asCTokenizer::GetDefinition(ttImport)), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->SetToken(&t);
	node->UpdateSourcePos(t.pos, t.length);

	asASTDeclId importDecl;
	node->AddChildLast(ParseFunctionDefinition(&importDecl));
	if( isSyntaxError ) return node;
	if( sema && importDecl.IsValid() )
	{
		// Non-semantic Stage 2 producer bridge: Builder later receives this
		// exact snImport pointer and carries the already-built stable key onto
		// its prepared Runtime import shell. No import syntax is decoded here.
		sema->BindParsedDeclarationIdentity(
			MakeParseActionIdentity(sema, node), importDecl);
	}

	GetToken(&t);
	if( t.type != ttIdentifier )
	{
		Error(ExpectedToken(FROM_TOKEN), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	tempString.Assign(&script->code[t.pos], t.length);
	if( tempString != FROM_TOKEN )
	{
		Error(ExpectedToken(FROM_TOKEN), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);

	GetToken(&t);
	if( t.type != ttStringConstant )
	{
		Error(TXT_EXPECTED_STRING, &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	asCScriptNode *mod = CreateNode(snConstant);
	if( mod == 0 ) return 0;

	node->AddChildLast(mod);

	mod->SetToken(&t);
	mod->UpdateSourcePos(t.pos, t.length);

	if( sema && importDecl.IsValid() )
	{
		asSImportOriginAction action;
		if( script && script->code && t.length >= 2
			&& t.pos + t.length <= script->codeLength )
		{
			// Preserve the maintained grammar's existing quote stripping.
			action.origin.Assign(
				script->code + t.pos + 1,
				(asUINT)t.length - 2);
			action.beginOffset = (asUINT)(t.pos + 1);
			action.endOffset = (asUINT)(t.pos + t.length - 1);
		}
		sema->ActOnImportOriginAction(importDecl, action, script);
	}

	GetToken(&t);
	if( t.type != ttEndStatement )
	{
		Error(ExpectedToken(asCTokenizer::GetDefinition(ttEndStatement)), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);
	BindCompletedParsedDeclarationIdentity(sema, node, importDecl);

	return node;
}

// BNF:0: SCRIPT        ::= {IMPORT | ENUM | TYPEDEF | CLASS | MIXIN | LOCAL | INTERFACE | FUNCDEF | VIRTPROP | VAR | FUNC | NAMESPACE | ';'}
asCScriptNode *asCParser::ParseScript(bool inBlock)
{
	asCScriptNode *node = CreateNode(snScript);
	if( node == 0 ) return 0;

	// Determine type of node
	for(;;)
	{
		while( !isSyntaxError )
		{
			sToken tStart;
			GetToken(&tStart);

			// Optimize by skipping tokens 'shared', 'external', 'final', 'abstract' so they don't have to be checked in every condition
			sToken t1 = tStart;
			/*while (IdentifierIs(t1, SHARED_TOKEN) ||
				IdentifierIs(t1, EXTERNAL_TOKEN) ||
				IdentifierIs(t1, FINAL_TOKEN) ||
				IdentifierIs(t1, TIDY_TOKEN) ||
				IdentifierIs(t1, ABSTRACT_TOKEN))
				GetToken(&t1);*/
			RewindTo(&tStart);

			asCScriptNode* decl = 0;
			if( t1.type == ttImport )
				decl = ParseImport();
			else if( t1.type == ttEnum )
				decl = ParseEnumeration();	// Handle enumerations
			else if( t1.type == ttTypedef )
				decl = ParseTypedef();		// Handle primitive typedefs
			else if( t1.type == ttClass || t1.type == ttStruct )
				decl = ParseClass();
			else if( t1.type == ttInterface )
				decl = ParseInterface();
			else if( t1.type == ttMixin )
				decl = ParseMixin();
			else if( t1.type == ttFuncDef )
				decl = ParseFuncDef();
			else if( t1.type == ttLocal )
				decl = ParseLocal();
			else if( t1.type == ttConst || t1.type == ttScope || t1.type == ttAuto || IsDataType(t1) )
			{
				if( IsVirtualPropertyDecl() )
					decl = ParseVirtualPropertyDecl(false, false);
				else if( IsVarDecl() )
					decl = ParseDeclaration(false, true);
				else
					decl = ParseFunction();
			}
			else if( t1.type == ttEndStatement )
			{
				// Ignore a semicolon by itself
				GetToken(&t1);
			}
			else if( t1.type == ttNamespace )
				decl = ParseNamespace();
			if( decl )
			{
				node->AddChildLast(decl);
			}
			else if( t1.type == ttImport
				|| t1.type == ttEnum
				|| t1.type == ttTypedef
				|| t1.type == ttClass || t1.type == ttStruct
				|| t1.type == ttInterface
				|| t1.type == ttMixin
				|| t1.type == ttFuncDef
				|| t1.type == ttLocal
				|| t1.type == ttConst || t1.type == ttScope || t1.type == ttAuto || IsDataType(t1)
				|| t1.type == ttNamespace )
			{
				// Parse* returned null; keep going through error recovery.
			}
			else if( t1.type == ttEndStatement )
			{
			}
			else if( t1.type == ttEnd )
				return node;
			else if( inBlock && t1.type == ttEndStatementBlock )
				return node;
			else
			{
				asCString str;
				const char *t = asCTokenizer::GetDefinition(t1.type);
				if( t == 0 ) t = "<unknown token>";

				str.Format(TXT_UNEXPECTED_TOKEN_s, t);

				Error(str, &t1);
			}
		}

		if( isSyntaxError )
		{
			// Search for either ';' or '{' or end
			sToken t1;
			GetToken(&t1);
			while( t1.type != ttEndStatement && t1.type != ttEnd &&
				   t1.type != ttStartStatementBlock )
				GetToken(&t1);

			if( t1.type == ttStartStatementBlock )
			{
				// Find the end of the block and skip nested blocks
				int level = 1;
				while( level > 0 )
				{
					GetToken(&t1);
					if( t1.type == ttStartStatementBlock ) level++;
					if( t1.type == ttEndStatementBlock ) level--;
					if( t1.type == ttEnd ) break;
				}
			}

			isSyntaxError = false;
		}
	}
	UNREACHABLE_RETURN;
}

// BNF:1: NAMESPACE     ::= 'namespace' IDENTIFIER '{' SCRIPT '}'
asCScriptNode *asCParser::ParseNamespace()
{
	asCScriptNode *node = CreateNode(snNamespace);
	if( node == 0 ) return 0;

	sToken t1;

	GetToken(&t1);
	if( t1.type == ttNamespace )
		node->UpdateSourcePos(t1.pos, t1.length);
	else
	{
		Error(ExpectedToken(asCTokenizer::GetDefinition(ttNamespace)), &t1);
		Error(InsteadFound(t1), &t1);
	}

	// Allow declaration of multiple nested namespace with namespace A::B::C { }
	// Keep the generic nodes for the LEGACY oracle, but copy only recognized
	// syntax facts into the Canonical Sema action payload.
	asCArray<asSNamespaceDeclActionSegment> namespacePath;
	asCScriptNode* identifier = ParseIdentifier();
	node->AddChildLast(identifier);
	if( isSyntaxError ) return node;
	if( identifier && script && script->code
		&& identifier->tokenLength > 0
		&& identifier->tokenPos + identifier->tokenLength <= script->codeLength )
	{
		asSNamespaceDeclActionSegment segment;
		segment.name.Assign(
			script->code + identifier->tokenPos,
			(asUINT)identifier->tokenLength);
		segment.beginOffset = (asUINT)identifier->tokenPos;
		segment.endOffset = (asUINT)(identifier->tokenPos + identifier->tokenLength);
		namespacePath.PushLast(segment);
	}

	asSDeclContextScope nsScope(sema);

	GetToken(&t1);
	while (t1.type == ttScope)
	{
		identifier = ParseIdentifier();
		node->AddChildLast(identifier);
		if( isSyntaxError ) return node;
		if( identifier && script && script->code
			&& identifier->tokenLength > 0
			&& identifier->tokenPos + identifier->tokenLength <= script->codeLength )
		{
			asSNamespaceDeclActionSegment segment;
			segment.name.Assign(
				script->code + identifier->tokenPos,
				(asUINT)identifier->tokenLength);
			segment.beginOffset = (asUINT)identifier->tokenPos;
			segment.endOffset = (asUINT)(identifier->tokenPos + identifier->tokenLength);
			namespacePath.PushLast(segment);
		}

		GetToken(&t1);
	}

	if( t1.type == ttStartStatementBlock )
		node->UpdateSourcePos(t1.pos, t1.length);
	else
	{
		Error(ExpectedToken(asCTokenizer::GetDefinition(ttStartStatementBlock)), &t1);
		Error(InsteadFound(t1), &t1);
		return node;
	}

	sToken start = t1;
	// The semantic namespace context must represent the complete qualified
	// path. Publish the typed action before the body so nested declarations see
	// the final scope even when a later body token fails to parse.
	if( sema )
	{
		sema->ActOnNamespacePath(namespacePath, script);
		nsScope.PushLastActed();
	}

	node->AddChildLast(ParseScript(true));

	if( !isSyntaxError )
	{
		GetToken(&t1);
		if( t1.type == ttEndStatementBlock )
			node->UpdateSourcePos(t1.pos, t1.length);
		else
		{
			if( t1.type == ttEnd )
				Error(TXT_UNEXPECTED_END_OF_FILE, &t1);
			else
			{
				Error(ExpectedToken(asCTokenizer::GetDefinition(ttEndStatementBlock)), &t1);
				Error(InsteadFound(t1), &t1);
			}
			Info(TXT_WHILE_PARSING_NAMESPACE, &start);
			return node;
		}
	}

	return node;
}

int asCParser::ParseStatementBlock(asCScriptCode *in_script, asCScriptNode *in_block)
{
	TimeIt("asCParser::ParseStatementBlock");

	Reset();

	// Tell the parser to validate the identifiers as valid types
	checkValidTypes = true;

	this->script = in_script;
	sourcePos = in_block->tokenPos;

	scriptNode = ParseStatementBlock();

	if( isSyntaxError || errorWhileParsing )
		return -1;

	return 0;
}

int asCParser::ParseStatement(asCScriptCode *in_script, asCScriptNode *in_node)
{
	TimeIt("asCParser::ParseStatement");

	Reset();

	// Tell the parser to validate the identifiers as valid types
	checkValidTypes = true;

	this->script = in_script;
	sourcePos = in_node->tokenPos;

	scriptNode = ParseStatement();

	if( isSyntaxError || errorWhileParsing )
		return -1;

	return 0;
}

// BNF:1: ENUM          ::= {'shared' | 'external'} 'enum' IDENTIFIER (';' | ('{' IDENTIFIER ['=' EXPR] {',' IDENTIFIER ['=' EXPR]} '}'))
asCScriptNode *asCParser::ParseEnumeration()
{
	asCScriptNode *ident;
	asCScriptNode *dataType;

	asCScriptNode *node = CreateNode(snEnum);
	if( node == 0 ) return 0;

	sToken token;

	// Optional 'shared' and 'external' token
	GetToken(&token);
	/*while( IdentifierIs(token, SHARED_TOKEN) ||
		   IdentifierIs(token, EXTERNAL_TOKEN) )
	{
		RewindTo(&token);
		node->AddChildLast(ParseIdentifier());
		if( isSyntaxError ) return node;

		GetToken(&token);
	}*/

	// Check for enum
	if( token.type != ttEnum )
	{
		Error(ExpectedToken(asCTokenizer::GetDefinition(ttEnum)), &token);
		Error(InsteadFound(token), &token);
		return node;
	}

	node->SetToken(&token);
	node->UpdateSourcePos(token.pos, token.length);

	// Get the identifier
	GetToken(&token);
	if(ttIdentifier != token.type)
	{
		Error(TXT_EXPECTED_IDENTIFIER, &token);
		Error(InsteadFound(token), &token);
		return node;
	}

	dataType = CreateNode(snDataType);
	if( dataType == 0 ) return node;

	node->AddChildLast(dataType);

	ident = CreateNode(snIdentifier);
	if( ident == 0 ) return node;

	ident->SetToken(&token);
	ident->UpdateSourcePos(token.pos, token.length);
	dataType->AddChildLast(ident);

	asSDeclContextScope enumScope(sema);
	asASTDeclId canonicalEnum;
	if( sema )
	{
		asSEnumDeclActionName enumName;
		enumName.name.Assign(
			script->code + ident->tokenPos,
			(asUINT)ident->tokenLength);
		enumName.beginOffset = (asUINT)ident->tokenPos;
		enumName.endOffset = (asUINT)(ident->tokenPos + ident->tokenLength);
		canonicalEnum = sema->ActOnEnumName(enumName, script);
		enumScope.PushLastActed();
	}

	// External shared declarations are ended with ';'
	GetToken(&token);
	if (token.type == ttEndStatement)
	{
		RewindTo(&token);
		node->AddChildLast(ParseToken(ttEndStatement));
		return node;
	}

	// check for the start of the declaration block
	if( token.type != ttStartStatementBlock )
	{
		RewindTo(&token);
		int tokens[] = { ttStartStatementBlock, ttEndStatement };
		Error(ExpectedOneOf(tokens, 2), &token);
		Error(InsteadFound(token), &token);
		return node;
	}

	while(ttEnd != token.type)
	{
		GetToken(&token);

		if( ttEndStatementBlock == token.type )
		{
			RewindTo(&token);
			break;
		}

		if(ttIdentifier != token.type)
		{
			Error(TXT_EXPECTED_IDENTIFIER, &token);
			Error(InsteadFound(token), &token);
			return node;
		}

		// Add the enum element
		ident = CreateNode(snIdentifier);
		if( ident == 0 ) return node;

		ident->SetToken(&token);
		ident->UpdateSourcePos(token.pos, token.length);
		node->AddChildLast(ident);
		asASTDeclId canonicalEnumerator;
		if( sema )
		{
			asSEnumDeclActionName enumeratorName;
			enumeratorName.name.Assign(
				script->code + ident->tokenPos,
				(asUINT)ident->tokenLength);
			enumeratorName.beginOffset = (asUINT)ident->tokenPos;
			enumeratorName.endOffset =
				(asUINT)(ident->tokenPos + ident->tokenLength);
			canonicalEnumerator = sema->ActOnEnumeratorName(
				canonicalEnum, enumeratorName, script);
			BindCompletedParsedDeclarationIdentity(
				sema, ident, canonicalEnumerator);
		}

		GetToken(&token);

		if( token.type == ttAssignment )
		{
			asCScriptNode	*tmp;

			RewindTo(&token);

			// The legacy builder deliberately keeps enum initializers
			// superficial and reparses them while compiling constants.  Canonical
			// Sema has no later syntax reparse: it must receive the complete RHS
			// now, otherwise a spelling such as `-2` is reduced to a fake
			// snConstant containing only the '-' token and the provisional
			// implicit enum value leaks into the sealed AST.
			if( sema )
			{
				const sToken assignmentToken = token;
				GetToken(&token); // '='
				GetToken(&token); // first RHS token
				RewindTo(&token);
				if( token.type == ttStartStatementBlock )
				{
					tmp = ParseInitList();
				}
				else
				{
					tmp = ParseConditionAsAssignment();
				}
				if( tmp )
				{
					// Preserve the historical initializer range anchored at '=';
					// legacy consumers and source diagnostics use that boundary.
					tmp->UpdateSourcePos(
						assignmentToken.pos, assignmentToken.length);
				}
			}
			else
			{
				tmp = SuperficiallyParseVarInit();
			}

			node->AddChildLast(tmp);
			if( isSyntaxError ) return node;
			if( sema )
			{
				asSEnumeratorInitializerAction action;
				action.enumerator = canonicalEnumerator;
				action.expression =
					FindSingleExactParsedExpressionIdentity(sema, tmp);
				action.recovered = isSyntaxError;
				BuildCopiedActionRange(
					tmp, script, action.beginOffset, action.endOffset);
				sema->ActOnEnumeratorInitializerAction(action, script);
			}
			GetToken(&token);
		}

		if(ttListSeparator != token.type)
		{
			RewindTo(&token);
			break;
		}
	}

	// check for the end of the declaration block
	GetToken(&token);
	if( token.type != ttEndStatementBlock )
	{
		RewindTo(&token);
		Error(ExpectedToken("}"), &token);
		Error(InsteadFound(token), &token);
		return node;
	}

	return node;
}

bool asCParser::IsAccessDecl()
{
	sToken t;
	GetToken(&t);

	if (t.type != ttAccess)
	{
		RewindTo(&t);
		return false;
	}

	sToken t1;

	GetToken(&t1);
	if (t1.type != ttIdentifier)
	{
		RewindTo(&t);
		return false;
	}

	GetToken(&t1);
	if (t1.type != ttAssignment)
	{
		RewindTo(&t);
		return false;
	}

	RewindTo(&t);
	return true;
}

asCScriptNode* asCParser::ParseAccessDecl()
{
	asCScriptNode *node = CreateNode(snAccessDeclaration);
	if( node == 0 ) return 0;

	sToken t1;
	GetToken(&t1);

	if (t1.type != ttAccess)
	{
		RewindTo(&t1);
		return 0;
	}
	asSAccessSpecifierAction canonicalAction;
	asASTDeclId canonicalDeclaration;
	canonicalAction.beginOffset = (asUINT)t1.pos;

	auto* identifierNode = ParseIdentifier();
	if (isSyntaxError) return node;
	if( identifierNode && script && script->code
		&& identifierNode->tokenLength > 0
		&& identifierNode->tokenPos + identifierNode->tokenLength
			<= script->codeLength )
	{
		canonicalAction.name.Assign(
			script->code + identifierNode->tokenPos,
			identifierNode->tokenLength);
	}

	ParseToken(ttAssignment);
	if (isSyntaxError) return node;

	node->AddChildLast(identifierNode);

	// Access must start from either private or protected
	GetToken(&t1);
	if (t1.type == ttPrivate)
	{
		RewindTo(&t1);
		node->AddChildLast(ParseToken(ttPrivate));
		canonicalAction.traits = asAST_TRAIT_PRIVATE;
	}
	else if (t1.type == ttProtected)
	{
		RewindTo(&t1);
		node->AddChildLast(ParseToken(ttProtected));
		canonicalAction.traits = asAST_TRAIT_PROTECTED;
	}
	else
	{
		const char *tokens[] = {"private", "protected"};
		Error(ExpectedOneOf(tokens, 2), &t1);
		Error(InsteadFound(t1), &t1);
		return node;
	}

	while (true)
	{
		GetToken(&t1);
		if (t1.type == ttListSeparator)
		{
			// Add another term in the acess declaration list
			GetToken(&t1);
			asSAccessPermissionAction permission;
			permission.beginOffset = (asUINT)t1.pos;

			asCScriptNode* accessNode = nullptr;
			if (t1.type == ttStar)
			{
				RewindTo(&t1);

				accessNode = ParseToken(ttStar);
				node->AddChildLast(accessNode);
				permission.name = "*";
				permission.traits |= asAST_TRAIT_ACCESS_WILDCARD;
				permission.endOffset = (asUINT)(t1.pos + t1.length);
			}
			else
			{
				RewindTo(&t1);
				auto* idNode = ParseIdentifier();
				if (isSyntaxError) return node;
				if( idNode && script && script->code
					&& idNode->tokenLength > 0
					&& idNode->tokenPos + idNode->tokenLength
						<= script->codeLength )
				{
					permission.name.Assign(
						script->code + idNode->tokenPos,
						idNode->tokenLength);
					permission.endOffset = (asUINT)(
						idNode->tokenPos + idNode->tokenLength);
				}

				accessNode = CreateNode(snUndefined);
				accessNode->AddChildLast(idNode);
				node->AddChildLast(accessNode);
			}

			// Optionally we can specify readonly or editdefaults on the class
			GetToken(&t1);
			if (t1.type == ttOpenParanthesis)
			{
				while (true)
				{
					GetToken(&t1);
					if (t1.type == ttCloseParanthesis)
					{
						break;
					}
					else if (IdentifierIs(t1, READONLY_TOKEN))
					{
						permission.traits |= asAST_TRAIT_ACCESS_READONLY;
						RewindTo(&t1);
						asCScriptNode* modifier = ParseIdentifier();
						accessNode->AddChildLast(modifier);
						permission.endOffset = (asUINT)(
							modifier->tokenPos + modifier->tokenLength);
					}
					else if (IdentifierIs(t1, EDITDEFAULTS_TOKEN))
					{
						permission.traits |= asAST_TRAIT_ACCESS_EDITDEFAULTS;
						RewindTo(&t1);
						asCScriptNode* modifier = ParseIdentifier();
						accessNode->AddChildLast(modifier);
						permission.endOffset = (asUINT)(
							modifier->tokenPos + modifier->tokenLength);
					}
					else if (IdentifierIs(t1, INHERITED_TOKEN))
					{
						// The Runtime Builder historically ignores inherited on '*'.
						// Preserve that exact language behavior in the typed facts.
						if( (permission.traits
							& asAST_TRAIT_ACCESS_WILDCARD) == 0 )
						{
							permission.traits |= asAST_TRAIT_ACCESS_INHERITED;
						}
						RewindTo(&t1);
						asCScriptNode* modifier = ParseIdentifier();
						accessNode->AddChildLast(modifier);
						permission.endOffset = (asUINT)(
							modifier->tokenPos + modifier->tokenLength);
					}
					else
					{
						const char *tokens[] = {READONLY_TOKEN, EDITDEFAULTS_TOKEN, INHERITED_TOKEN, ")"};
						Error(ExpectedOneOf(tokens, 4), &t1);
						Error(InsteadFound(t1), &t1);
						return node;
					}

					GetToken(&t1);
					if (t1.type == ttCloseParanthesis)
					{
						permission.endOffset = (asUINT)(t1.pos + t1.length);
						break;
					}
					else if (t1.type == ttListSeparator)
					{
						continue;
					}
					else
					{
						const char *tokens[] = {",", ")"};
						Error(ExpectedOneOf(tokens, 2), &t1);
						Error(InsteadFound(t1), &t1);
						return node;
					}
				}
			}
			else
			{
				RewindTo(&t1);
			}
			const asUINT permissionIndex =
				canonicalAction.permissions.GetLength();
			bool injectAppendFailure = false;
#if WITH_ANGELSCRIPT_UNITTESTS
			injectAppendFailure = builder != 0
				&& builder->ShouldInjectTestCanonicalAccessPermissionAppendFailure();
#endif
			if( injectAppendFailure
				|| !canonicalAction.permissions.SetLength(permissionIndex + 1) )
			{
				Error("Out of memory while recording Canonical access permission", &t1);
				return node;
			}
			canonicalAction.permissions[permissionIndex] = permission;
			continue;
		}
		else if (t1.type == ttEndStatement)
		{
			// Access declaration ended
			canonicalAction.endOffset = (asUINT)(t1.pos + t1.length);
			if( sema )
			{
				canonicalDeclaration =
					sema->ActOnAccessSpecifierAction(canonicalAction, script);
			}
			break;
		}
		else
		{
			const char *tokens[] = {",", ";"};
			Error(ExpectedOneOf(tokens, 2), &t1);
			Error(InsteadFound(t1), &t1);
			return node;
		}
	}
	BindCompletedParsedDeclarationIdentity(
		sema, node, canonicalDeclaration);

	return node;
}

bool asCParser::IsClassDefaultStatement()
{
	sToken t;
	GetToken(&t);

	if (t.type != ttDefault)
	{
		RewindTo(&t);
		return false;
	}

	RewindTo(&t);
	return true;
}

asCScriptNode* asCParser::ParseClassDefaultStatement()
{
	asCScriptNode *node = CreateNode(snClassDefaultStatement);
	if( node == 0 ) return 0;

	sToken t1;
	GetToken(&t1);

	if (t1.type != ttDefault)
	{
		RewindTo(&t1);
		return 0;
	}

	node->SetToken(&t1);
	asASTDeclId initDefaults;
	asSDeclContextScope initDefaultsScope(sema);
	if( sema )
	{
		initDefaults = sema->ActOnClassDefaultStartAction(
			sema->CurrentDeclContext(), script,
			(asUINT)t1.pos, (asUINT)(t1.pos + t1.length));
		initDefaultsScope.Push(initDefaults);
	}

	// LEGACY still reparses this syntax in asCCompiler. Canonical Sema instead
	// authors the statement once, directly under the generated method selected
	// above, then routes only its exact typed identity into the method body.
	asCScriptNode* const statement = sema
		? ParseStatement() : SuperficiallyParseStatement();
	node->AddChildLast(statement);
	if( sema && initDefaults.IsValid() && statement != 0 && !isSyntaxError )
	{
		sema->ActOnClassDefaultStatementAction(
			initDefaults, script,
			(asUINT)statement->tokenPos,
			(asUINT)(statement->tokenPos + statement->tokenLength));
	}
	return node;
}

bool asCParser::IsVarDecl()
{
	// Set start point so that we can rewind
	sToken t;
	GetToken(&t);
	RewindTo(&t);

	// A class property decl can be preceded by 'private' or 'protected'
	sToken t1;
	GetToken(&t1);

	if (t1.type == ttAccess)
	{
		// Access specifiers followed by a colon are variable decls,
		// followed by an identifier they are access decls
		sToken taccess;

		GetToken(&taccess);
		if (taccess.type != ttColon)
		{
			RewindTo(&t1);
			return false;
		}

		GetToken(&taccess);
		if (taccess.type != ttIdentifier)
		{
			RewindTo(&t1);
			return false;
		}
	}
	else if( t1.type != ttPrivate && t1.type != ttProtected )
		RewindTo(&t1);

	// A variable decl starts with the type
	if (!IsType(t1))
	{
		RewindTo(&t);
		return false;
	}

	// Jump to the token after the type
	RewindTo(&t1);
	GetToken(&t1);
	
	// The declaration needs to have a name
	if( t1.type != ttIdentifier )
	{
		RewindTo(&t);
		return false;
	}

	// It can be followed by an initialization
	GetToken(&t1);
	if( t1.type == ttEndStatement || t1.type == ttAssignment || t1.type == ttListSeparator )
	{
		RewindTo(&t);
		return true;
	}
	if( t1.type == ttOpenParanthesis )
	{
		// If the closing paranthesis is followed by a statement
		// block or end-of-file, then treat it as a function. A
		// function decl may have nested paranthesis so we need to
		// check for this too.
		int nest = 0;
		while( t1.type != ttEnd )
		{
			if( t1.type == ttOpenParanthesis )
				nest++;
			else if( t1.type == ttCloseParanthesis )
			{
				nest--;
				if( nest == 0 )
					break;
			}
			GetToken(&t1);
		}

		if (t1.type == ttEnd)
		{
			RewindTo(&t);
			return false;
		}
		else
		{
			GetToken(&t1);
			RewindTo(&t);
			if( t1.type == ttStartStatementBlock ||
				t1.type == ttIdentifier || // Function decorator
				t1.type == ttEnd)
				return false;
		}

		RewindTo(&t);
		return true;
	}

	RewindTo(&t);
	return false;
}

bool asCParser::IsVirtualPropertyDecl()
{
	// Set start point so that we can rewind
	sToken t;
	GetToken(&t);
	RewindTo(&t);

	// A class property decl can be preceded by 'private' or 'protected'
	sToken t1;
	GetToken(&t1);
	if( t1.type != ttPrivate && t1.type != ttProtected )
		RewindTo(&t1);

	// A variable decl starts with the type
	if (!IsType(t1))
	{
		RewindTo(&t);
		return false;
	}

	// Move to the token after the type
	RewindTo(&t1);
	GetToken(&t1);

	// The decl must have an identifier
	if( t1.type != ttIdentifier )
	{
		RewindTo(&t);
		return false;
	}

	// To be a virtual property it must also have a block for the get/set functions
	GetToken(&t1);
	if( t1.type == ttStartStatementBlock )
	{
		RewindTo(&t);
		return true;
	}

	RewindTo(&t);
	return false;
}

bool asCParser::IsFuncDecl(bool isMethod)
{
	// Set start point so that we can rewind
	sToken t;
	GetToken(&t);
	RewindTo(&t);

	if( isMethod )
	{
		// A class method decl can be preceded by 'private' or 'protected'
		sToken t1, t2;
		GetToken(&t1);

		if (t1.type == ttAccess)
		{
			// Access specifiers followed by a colon can be function decls,
			// followed by an identifier they are access decls
			sToken taccess;

			GetToken(&taccess);
			if (taccess.type != ttColon)
			{
				RewindTo(&t1);
				return false;
			}

			GetToken(&taccess);
			if (taccess.type != ttIdentifier)
			{
				RewindTo(&t1);
				return false;
			}
		}
		else if( t1.type != ttPrivate && t1.type != ttProtected )
			RewindTo(&t1);

		// A class constructor starts with identifier followed by parenthesis
		// A class destructor starts with the ~ token
		GetToken(&t1);
		GetToken(&t2);
		RewindTo(&t1);
		if( (t1.type == ttIdentifier && t2.type == ttOpenParanthesis) || t1.type == ttBitNot )
		{
			RewindTo(&t);
			return true;
		}
	}

	// A function decl starts with a type
	sToken t1;
	if (!IsType(t1))
	{
		RewindTo(&t);
		return false;
	}

	// Move to the token after the type
	RewindTo(&t1);
	GetToken(&t1);

	// There can be an ampersand if the function returns a reference
	if( t1.type == ttAmp )
	{
		RewindTo(&t);
		return true;
	}

	if( t1.type != ttIdentifier )
	{
		RewindTo(&t);
		return false;
	}

	GetToken(&t1);
	if( t1.type == ttOpenParanthesis )
	{
		// If the closing parenthesis is not followed by a
		// statement block then it is not a function.
		// It's possible that there are nested parenthesis due to default
		// arguments so this should be checked for.
		int nest = 0;
		GetToken(&t1);
		while( (nest || t1.type != ttCloseParanthesis) && t1.type != ttEnd )
		{
			if( t1.type == ttOpenParanthesis )
				nest++;
			if( t1.type == ttCloseParanthesis )
				nest--;

			GetToken(&t1);
		}

		if( t1.type == ttEnd )
			return false;
		else
		{
			if( isMethod )
			{
				// A class method can have a 'const' token after the parameter list
				GetToken(&t1);
				if( t1.type != ttConst )
					RewindTo(&t1);
			}

			// Global functions may carry the same post-parameter specifiers as
			// methods. `external_implicit_this` is how generated `__Init_*`
			// literal-asset functions are spelled; skipping them only for
			// methods made those globals look like non-functions.
			for( ; ; )
			{
				GetToken(&t1);
				if( IdentifierIs(t1, PROPERTY_TOKEN) )
				{
					Error(TXT_PROPERTY_DECORATOR_REMOVED, &t1);
					return false;
				}
				if( !IdentifierIs(t1, FINAL_TOKEN)
					&& !IdentifierIs(t1, OVERRIDE_TOKEN)
					&& !IdentifierIs(t1, MIXIN_TOKEN)
					&& !IdentifierIs(t1, ACCEPT_TEMPORARY_TOKEN)
					&& !IdentifierIs(t1, EXTERNAL_IMPLICIT_THIS_TOKEN)
					&& !IdentifierIs(t1, NO_DISCARD_TOKEN)
					&& !IdentifierIs(t1, ALLOW_DISCARD_TOKEN)
					&& !IdentifierIs(t1, GENERATED_FUNCTION_TOKEN)
					&& !IdentifierIs(t1, DEPRECATED_TOKEN)
					&& !IdentifierIs(t1, DEFAULTS_TOKEN)
					&& !IdentifierIs(t1, UNSAFE_DURING_CONSTRUCTION_TOKEN)
				)
				{
					RewindTo(&t1);
					break;
				}
			}

			GetToken(&t1);
			RewindTo(&t);
			if( t1.type == ttStartStatementBlock )
				return true;
		}

		RewindTo(&t);
		return false;
	}

	RewindTo(&t);
	return false;
}

// BNF:1: FUNCDEF       ::= {'external' | 'shared'} 'funcdef' TYPE ['&'] IDENTIFIER PARAMLIST ';'
asCScriptNode *asCParser::ParseFuncDef()
{
	asCScriptNode *node = CreateNode(snFuncDef);
	if( node == 0 ) return 0;

	// Allow keywords 'external' and 'shared' before 'interface'
	sToken t1;
	GetToken(&t1);
	/*while (IdentifierIs(t1, SHARED_TOKEN) ||
		   IdentifierIs(t1, EXTERNAL_TOKEN))
	{
		RewindTo(&t1);
		node->AddChildLast(ParseIdentifier());
		if (isSyntaxError) return node;

		GetToken(&t1);
	}*/

	if( t1.type != ttFuncDef )
	{
		Error(asCTokenizer::GetDefinition(ttFuncDef), &t1);
		return node;
	}

	node->SetToken(&t1);

	node->AddChildLast(ParseType(true));
	if( isSyntaxError ) return node;

	node->AddChildLast(ParseTypeMod(false));
	if( isSyntaxError ) return node;

	asCScriptNode* const nameNode = ParseIdentifier();
	node->AddChildLast(nameNode);
	if( isSyntaxError ) return node;

	// Funcdef remains fork-rejected at the tokenizer boundary, but this retained
	// internal Parser family must still obey the Canonical action contract. If
	// invoked by an internal/application path, publish the callable type identity
	// before parameters and enter the exact FuncDef DeclContext. The growing
	// snFuncDef is LEGACY/recovery storage only.
	asSDeclContextScope funcDefScope(sema);
	asASTDeclId canonicalFuncDef;
	if( sema )
	{
		asSFuncDefSignatureAction action;
		if( nameNode && script && script->code
			&& nameNode->tokenLength > 0
			&& nameNode->tokenPos + nameNode->tokenLength <= script->codeLength )
		{
			action.name.Assign(
				script->code + nameNode->tokenPos,
				(asUINT)nameNode->tokenLength);
			action.beginOffset = (asUINT)nameNode->tokenPos;
			action.endOffset =
				(asUINT)(nameNode->tokenPos + nameNode->tokenLength);
		}
		action.returnType = sema->ActOnQualTypeAction(
			BuildQualTypeSyntaxAction(
				node->firstChild,
				node->firstChild ? node->firstChild->next : 0), script);
		canonicalFuncDef = sema->ActOnFuncDefSignatureAction(action, script);
		sema->BindParsedDeclarationIdentity(
			MakeParseActionIdentity(sema, node), canonicalFuncDef);
		funcDefScope.Push(canonicalFuncDef);
	}

	node->AddChildLast(ParseParameterList(canonicalFuncDef));
	if( isSyntaxError ) return node;

	GetToken(&t1);
	if( t1.type != ttEndStatement )
	{
		Error(ExpectedToken(asCTokenizer::GetDefinition(ttEndStatement)), &t1);
		Error(InsteadFound(t1), &t1);
		return node;
	}

	node->UpdateSourcePos(t1.pos, t1.length);
	BindCompletedParsedDeclarationIdentity(sema, node, canonicalFuncDef);

	return node;
}

// BNF:1: FUNC          ::= {'shared' | 'external' } ['private' | 'protected'] [((TYPE ['&']) | '~')] IDENTIFIER PARAMLIST ['const'] {'override' | 'final'} (';' | STATBLOCK)
asCScriptNode *asCParser::ParseFunction(bool isMethod, bool notifySemaAfterParams, asCScriptNode* prefixChild)
{
	asCScriptNode *node = CreateNode(snFunction);
	if( node == 0 ) return 0;
	const bool isMixin = prefixChild && prefixChild->tokenType == ttMixin;
	asDWORD canonicalTraits = isMixin ? asAST_TRAIT_MIXIN : 0;
	asCString accessSpecifierName;
	if( prefixChild )
	{
		node->AddChildLast(prefixChild);
	}

	sToken t1;
	GetToken(&t1);
	/*if (!isMethod)
	{
		// A global function can be marked as shared and external
		while (t1.type == ttIdentifier)
		{
			if (IdentifierIs(t1, SHARED_TOKEN) ||
				IdentifierIs(t1, EXTERNAL_TOKEN))
			{
				RewindTo(&t1);
				node->AddChildLast(ParseIdentifier());
				if (isSyntaxError) return node;
			}
			else
				break;

			GetToken(&t1);
		}
	}*/

	// A class method can start with 'private' or 'protected'
	if (isMethod)
	{
		if (t1.type == ttPrivate)
		{
			RewindTo(&t1);
			node->AddChildLast(ParseToken(ttPrivate));
			canonicalTraits |= asAST_TRAIT_PRIVATE;
			GetToken(&t1);
		}
		else if (t1.type == ttProtected)
		{
			RewindTo(&t1);
			node->AddChildLast(ParseToken(ttProtected));
			canonicalTraits |= asAST_TRAIT_PROTECTED;
			GetToken(&t1);
		}
		else if (t1.type == ttAccess)
		{
			RewindTo(&t1);
			auto* accessNode = ParseToken(ttAccess);

			sToken topen;
			GetToken(&topen);
			if( topen.type != ttColon )
			{
				Error(ExpectedToken(":"), &topen);
				Error(InsteadFound(topen), &topen);
				return node;
			}

			auto* identifier = ParseIdentifier();
			if (isSyntaxError) return node;
			if( identifier && script && script->code
				&& identifier->tokenLength > 0
				&& identifier->tokenPos + identifier->tokenLength
					<= script->codeLength )
			{
				accessSpecifierName.Assign(
					script->code + identifier->tokenPos,
					identifier->tokenLength);
			}

			accessNode->AddChildLast(identifier);
			node->AddChildLast(accessNode);

			GetToken(&t1);
		}
	}

	if( isSyntaxError ) return node;

	// If it is a global function, or a method, except constructor and destructor, then the return type is parsed
	sToken t2;
	GetToken(&t2);
	RewindTo(&t1);
	asCScriptNode* typeNode = 0;
	asCScriptNode* typeMod = 0;
	if( !isMethod || (t1.type != ttBitNot && t2.type != ttOpenParanthesis) )
	{
		typeNode = ParseType(true);
		node->AddChildLast(typeNode);
		if( isSyntaxError ) return node;

		typeMod = ParseTypeMod(false);
		node->AddChildLast(typeMod);
		if( isSyntaxError ) return node;
	}

	// If this is a class destructor then it starts with ~, and no return type is declared
	const bool isDestructor = isMethod && t1.type == ttBitNot;
	if( isDestructor )
	{
		node->AddChildLast(ParseToken(ttBitNot));
		if( isSyntaxError ) return node;
	}

	asCScriptNode* const nameNode = ParseIdentifier();
	node->AddChildLast(nameNode);
	if( isSyntaxError ) return node;

	// Clang-shaped: publish recognized function identity and canonical return
	// type immediately after the name. Parameters then enter this exact function
	// DeclContext; the growing snFunction remains LEGACY/recovery storage only.
	asSDeclContextScope fnScope(sema);
	asASTDeclId canonicalFunction;
	if( sema && notifySemaAfterParams )
	{
		asSFunctionSignatureAction action;
		if( nameNode && script && script->code
			&& nameNode->tokenLength > 0
			&& nameNode->tokenPos + nameNode->tokenLength <= script->codeLength )
		{
			action.name.Assign(
				script->code + nameNode->tokenPos,
				(asUINT)nameNode->tokenLength);
			action.beginOffset = typeNode
				? (asUINT)typeNode->tokenPos
				: (asUINT)nameNode->tokenPos;
			action.endOffset =
				(asUINT)(nameNode->tokenPos + nameNode->tokenLength);
		}
		if( typeNode )
		{
			action.returnType = sema->ActOnQualTypeAction(
				BuildQualTypeSyntaxAction(typeNode, typeMod), script);
		}
		action.kind = isDestructor
			? asFUNCTION_ACTION_DESTRUCTOR
			: (isMixin
				? asFUNCTION_ACTION_MIXIN
				: (isMethod ? asFUNCTION_ACTION_METHOD : asFUNCTION_ACTION_FUNCTION));
		action.accessSpecifierName = accessSpecifierName;
		canonicalFunction = sema->ActOnFunctionSignatureAction(action, script);
		sema->BindParsedDeclarationIdentity(
			MakeParseActionIdentity(sema, node), canonicalFunction);
		fnScope.Push(canonicalFunction);
	}

	node->AddChildLast(ParseParameterList(canonicalFunction));
	if( isSyntaxError ) return node;

	if( isMethod )
	{
		GetToken(&t1);
		RewindTo(&t1);

		// Is the method a const?
		if( t1.type == ttConst )
		{
			node->AddChildLast(ParseToken(ttConst));
			canonicalTraits |= asAST_TRAIT_CONST_METHOD;
		}
	}

	// TODO: Should support abstract methods, in which case no statement block should be provided
	ParseMethodAttributes(node, &canonicalTraits); // Parses "property" keyword for global functions as well
	if (isSyntaxError)
		return node;
	if( sema && canonicalFunction.IsValid() )
	{
		sema->ActOnFunctionTraitsAction(canonicalFunction, canonicalTraits);
	}

	// External shared functions must be ended with ';'
	GetToken(&t1);
	RewindTo(&t1);
	if (t1.type == ttEndStatement)
	{
		node->AddChildLast(ParseToken(ttEndStatement));
		BindCompletedParsedDeclarationIdentity(sema, node, canonicalFunction);
		return node;
	}

	// Canonical Sema needs a real statement tree; the compiler still re-parses
	// from the block span when the legacy pipeline is selected.
	asCScriptNode* const body = ParseFunctionStatementBlock();
	node->AddChildLast(body);
	if( sema && canonicalFunction.IsValid() && body && !isSyntaxError )
	{
		asSFunctionBodyAction bodyAction;
		bodyAction.function = canonicalFunction;
		bodyAction.beginOffset = (asUINT)body->tokenPos;
		bodyAction.endOffset =
			(asUINT)(body->tokenPos + body->tokenLength);
		bodyAction.recovered = isSyntaxError;
		bodyAction.body = FindExactParsedStatementIdentity(body);
		sema->ActOnFunctionBodyAction(bodyAction, script);
	}
	BindCompletedParsedDeclarationIdentity(sema, node, canonicalFunction);

	return node;
}

// BNF:2: INTFMTHD      ::= TYPE ['&'] IDENTIFIER PARAMLIST ['const'] ';'
asCScriptNode *asCParser::ParseInterfaceMethod()
{
	asCScriptNode *node = CreateNode(snFunction);
	if( node == 0 ) return 0;

	asCScriptNode* const typeNode = ParseType(true);
	node->AddChildLast(typeNode);
	if( isSyntaxError ) return node;

	asCScriptNode* const typeMod = ParseTypeMod(false);
	node->AddChildLast(typeMod);
	if( isSyntaxError ) return node;

	asCScriptNode* const nameNode = ParseIdentifier();
	node->AddChildLast(nameNode);
	if( isSyntaxError ) return node;

	asSDeclContextScope methodScope(sema);
	asASTDeclId canonicalMethod;
	if( sema )
	{
		asSFunctionSignatureAction action;
		if( nameNode && script && script->code
			&& nameNode->tokenLength > 0
			&& nameNode->tokenPos + nameNode->tokenLength <= script->codeLength )
		{
			action.name.Assign(
				script->code + nameNode->tokenPos,
				(asUINT)nameNode->tokenLength);
			action.beginOffset = typeNode
				? (asUINT)typeNode->tokenPos
				: (asUINT)nameNode->tokenPos;
			action.endOffset =
				(asUINT)(nameNode->tokenPos + nameNode->tokenLength);
		}
		action.returnType = sema->ActOnQualTypeAction(
			BuildQualTypeSyntaxAction(typeNode, typeMod), script);
		action.kind = asFUNCTION_ACTION_METHOD;
		canonicalMethod = sema->ActOnFunctionSignatureAction(action, script);
		sema->BindParsedDeclarationIdentity(
			MakeParseActionIdentity(sema, node), canonicalMethod);
		methodScope.Push(canonicalMethod);
	}

	node->AddChildLast(ParseParameterList(canonicalMethod));
	const bool parameterError = isSyntaxError;

	// Parse an optional const after the method definition
	sToken t1;
	asDWORD canonicalTraits = 0;
	if( !parameterError )
	{
		GetToken(&t1);
		RewindTo(&t1);
		if( t1.type == ttConst )
		{
			node->AddChildLast(ParseToken(ttConst));
			canonicalTraits |= asAST_TRAIT_CONST_METHOD;
		}
	}

	if( sema && canonicalMethod.IsValid() && !parameterError )
	{
		sema->ActOnFunctionTraitsAction(canonicalMethod, canonicalTraits);
	}
	if( parameterError ) return node;

	GetToken(&t1);
	if( t1.type != ttEndStatement )
	{
		Error(ExpectedToken(";"), &t1);
		Error(InsteadFound(t1), &t1);
		return node;
	}

	node->UpdateSourcePos(t1.pos, t1.length);
	BindCompletedParsedDeclarationIdentity(sema, node, canonicalMethod);

	return node;
}

// BNF:1: VIRTPROP      ::= ['private' | 'protected'] TYPE ['&'] IDENTIFIER '{' {('get' | 'set') ['const'] [('override' | 'final')] (STATBLOCK | ';')} '}'
asCScriptNode *asCParser::ParseVirtualPropertyDecl(bool isMethod, bool isInterface)
{
	(void)isMethod;
	(void)isInterface;

	sToken t1;
	GetToken(&t1);
	Error(TXT_VIRTUAL_PROPERTY_REMOVED, &t1);
	return 0;
}

// BNF:1: INTERFACE     ::= {'external' | 'shared'} 'interface' IDENTIFIER (';' | ([':' IDENTIFIER {',' IDENTIFIER}] '{' {VIRTPROP | INTFMTHD} '}'))
asCScriptNode *asCParser::ParseInterface()
{
	asCScriptNode *node = CreateNode(snInterface);
	if( node == 0 ) return 0;

	sToken t;

	// Allow keywords 'external' and 'shared' before 'interface'
	GetToken(&t);
	/*while( IdentifierIs(t, SHARED_TOKEN) ||
		   IdentifierIs(t, EXTERNAL_TOKEN) )
	{
		RewindTo(&t);
		node->AddChildLast(ParseIdentifier());
		if (isSyntaxError) return node;

		GetToken(&t);
	}*/

	if( t.type != ttInterface )
	{
		Error(ExpectedToken("interface"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->SetToken(&t);
	asCScriptNode* const nameNode = ParseIdentifier();
	node->AddChildLast(nameNode);
	if( isSyntaxError ) return node;

	asASTDeclId recordDecl;
	asSDeclContextScope ifaceScope(sema);
	if( sema )
	{
		asSRecordHeaderAction action;
		action.kind = asRECORD_ACTION_INTERFACE;
		if( nameNode != 0 && script != 0 && script->code != 0
			&& nameNode->tokenLength > 0
			&& nameNode->tokenPos + nameNode->tokenLength <= script->codeLength )
		{
			action.name.Assign(
				script->code + nameNode->tokenPos,
				(asUINT)nameNode->tokenLength);
			action.beginOffset = (asUINT)nameNode->tokenPos;
			action.endOffset =
				(asUINT)(nameNode->tokenPos + nameNode->tokenLength);
		}
		recordDecl = sema->ActOnRecordHeaderAction(action, script);
		if( recordDecl.IsValid() )
		{
			// Identity-only bridge for the transitional Builder producer. Record
			// syntax is already authoritative in the typed action above.
			sema->BindParsedDeclarationIdentity(
				MakeParseActionIdentity(sema, node), recordDecl);
			ifaceScope.Push(recordDecl);
		}
	}

	// External shared declarations are ended with ';'
	GetToken(&t);
	if (t.type == ttEndStatement)
	{
		RewindTo(&t);
		node->AddChildLast(ParseToken(ttEndStatement));
		BindCompletedParsedDeclarationIdentity(sema, node, recordDecl);
		return node;
	}

	asCArray<asSRecordBaseSpecifierAction> canonicalBases;

	// Can optionally have a list of interfaces that are inherited
	if( t.type == ttColon )
	{
		asCScriptNode *inherit = CreateNode(snIdentifier);
		node->AddChildLast(inherit);

		ParseOptionalScope(inherit);
		inherit->AddChildLast(ParseIdentifier());
		if( sema )
		{
			asSRecordBaseSpecifierAction action;
			if( BuildRecordBaseSpecifierAction(inherit, script, action) )
			{
				canonicalBases.PushLast(action);
			}
		}
		GetToken(&t);
		while( t.type == ttListSeparator )
		{
			inherit = CreateNode(snIdentifier);
			node->AddChildLast(inherit);

			ParseOptionalScope(inherit);
			inherit->AddChildLast(ParseIdentifier());
			if( sema )
			{
				asSRecordBaseSpecifierAction action;
				if( BuildRecordBaseSpecifierAction(inherit, script, action) )
				{
					canonicalBases.PushLast(action);
				}
			}
			GetToken(&t);
		}
	}

	// Publish the complete typed base list before method signatures. No
	// snInterface node crosses the Canonical Sema boundary.
	if( sema && recordDecl.IsValid() )
	{
		sema->ActOnRecordBaseSpecifiersAction(
			recordDecl, canonicalBases, script);
	}

	if( t.type != ttStartStatementBlock )
	{
		Error(ExpectedToken("{"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	// Parse interface methods
	GetToken(&t);
	RewindTo(&t);
	while( t.type != ttEndStatementBlock && t.type != ttEnd )
	{
		if( IsVirtualPropertyDecl() )
			node->AddChildLast(ParseVirtualPropertyDecl(true, true));
		else if( t.type == ttEndStatement )
			// Skip empty declarations
			GetToken(&t);
		else
			// Parse the method signature
			node->AddChildLast(ParseInterfaceMethod());

		if( isSyntaxError ) return node;

		GetToken(&t);
		RewindTo(&t);
	}

	GetToken(&t);
	if( t.type != ttEndStatementBlock )
	{
		Error(ExpectedToken("}"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);
	if( sema && recordDecl.IsValid() )
	{
		sema->ActOnRecordFinishAction(recordDecl);
	}
	BindCompletedParsedDeclarationIdentity(sema, node, recordDecl);

	return node;
}

// BNF:1: MIXIN         ::= 'mixin' FUNCTION
asCScriptNode *asCParser::ParseMixin()
{
	auto* tokenNode = ParseToken(ttMixin);
	if (tokenNode == nullptr)
		return nullptr;

	return ParseFunction(false, true, tokenNode);
}

// BNF:1: LOCAL         ::= 'local' FUNCTION
asCScriptNode* asCParser::ParseLocal()
{
	auto* tokenNode = ParseToken(ttLocal);
	if (tokenNode == nullptr)
		return nullptr;

	// Keep the local token as recovery syntax while using the ordinary typed
	// function action path and exact function DeclContext.
	return ParseFunction(false, true, tokenNode);
}

// BNF:1: CLASS         ::= {'shared' | 'abstract' | 'final' | 'external'} 'class' IDENTIFIER (';' | ([':' IDENTIFIER {',' IDENTIFIER}] '{' {VIRTPROP | FUNC | VAR | FUNCDEF} '}'))
asCScriptNode *asCParser::ParseClass()
{
	asCScriptNode *node = CreateNode(snClass);
	if( node == 0 ) return 0;

	sToken t;
	GetToken(&t);

	// Allow the keywords 'shared', 'abstract', 'final', and 'external' before 'class'
	/*while( IdentifierIs(t, SHARED_TOKEN) ||
		   IdentifierIs(t, ABSTRACT_TOKEN) ||
		   IdentifierIs(t, TIDY_TOKEN) ||
		   IdentifierIs(t, FINAL_TOKEN) ||
		   IdentifierIs(t, EXTERNAL_TOKEN) )
	{
		RewindTo(&t);
		node->AddChildLast(ParseIdentifier());
		GetToken(&t);
	}*/

	if( t.type != ttClass && t.type != ttStruct )
	{
		Error(ExpectedToken("class"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	bool isStruct = (t.type == ttStruct);
	node->SetToken(&t);

	if( engine->ep.allowImplicitHandleTypes )
	{
		// Parse 'implicit handle class' construct
		GetToken(&t);
		if ( t.type == ttHandleSuffix )
			node->SetToken(&t);
		else
			RewindTo(&t);
	}

	asCScriptNode* const nameNode = ParseIdentifier();
	node->AddChildLast(nameNode);
	if( isSyntaxError ) return node;

	// Publish class/struct identity and nominal type immediately after the
	// identifier, before bases or members.
	asASTDeclId recordDecl;
	asSDeclContextScope classScope(sema);
	if( sema )
	{
		asSRecordHeaderAction action;
		action.kind = isStruct
			? asRECORD_ACTION_STRUCT : asRECORD_ACTION_CLASS;
		if( nameNode != 0 && script != 0 && script->code != 0
			&& nameNode->tokenLength > 0
			&& nameNode->tokenPos + nameNode->tokenLength <= script->codeLength )
		{
			action.name.Assign(
				script->code + nameNode->tokenPos,
				(asUINT)nameNode->tokenLength);
			action.beginOffset = (asUINT)nameNode->tokenPos;
			action.endOffset =
				(asUINT)(nameNode->tokenPos + nameNode->tokenLength);
		}
		recordDecl = sema->ActOnRecordHeaderAction(action, script);
		if( recordDecl.IsValid() )
		{
			// Identity-only bridge for the transitional Builder producer. Record
			// syntax is already authoritative in the typed action above.
			sema->BindParsedDeclarationIdentity(
				MakeParseActionIdentity(sema, node), recordDecl);
			classScope.Push(recordDecl);
		}
	}

	// External shared declarations are ended with ';'
	GetToken(&t);
	if (t.type == ttEndStatement)
	{
		RewindTo(&t);
		node->AddChildLast(ParseToken(ttEndStatement));
		BindCompletedParsedDeclarationIdentity(sema, node, recordDecl);
		return node;
	}

	asCArray<asSRecordBaseSpecifierAction> canonicalBases;

	// Optional list of interfaces that are being implemented and classes that are being inherited
	if( t.type == ttColon )
	{
		asCScriptNode *inherit = CreateNode(snIdentifier);
		node->AddChildLast(inherit);

		ParseOptionalScope(inherit);
		inherit->AddChildLast(ParseIdentifier());
		if( sema )
		{
			asSRecordBaseSpecifierAction action;
			if( BuildRecordBaseSpecifierAction(inherit, script, action) )
			{
				canonicalBases.PushLast(action);
			}
		}
		GetToken(&t);
		while( t.type == ttListSeparator )
		{
			inherit = CreateNode(snIdentifier);
			node->AddChildLast(inherit);

			ParseOptionalScope(inherit);
			inherit->AddChildLast(ParseIdentifier());
			if( sema )
			{
				asSRecordBaseSpecifierAction action;
				if( BuildRecordBaseSpecifierAction(inherit, script, action) )
				{
					canonicalBases.PushLast(action);
				}
			}
			GetToken(&t);
		}
	}

	// Base specifiers are typed semantic facts of the record header. Publish
	// them before any field initializer or method body so `this` and
	// derived-to-base conversions see the authoritative inheritance graph.
	if( sema && recordDecl.IsValid() )
	{
		sema->ActOnRecordBaseSpecifiersAction(
			recordDecl, canonicalBases, script);
	}

	if( t.type != ttStartStatementBlock )
	{
		Error(ExpectedToken("{"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	// Parse properties
	GetToken(&t);
	RewindTo(&t);
	while( t.type != ttEndStatementBlock && t.type != ttEnd )
	{
		// Is it a property or a method?
		if (t.type == ttFuncDef)
		{
			node->AddChildLast(ParseFuncDef());
		}
		else if( IsFuncDecl(true) )
			node->AddChildLast(ParseFunction(true));
		else if( IsVirtualPropertyDecl() )
			node->AddChildLast(ParseVirtualPropertyDecl(true, false));
		else if( IsVarDecl() )
			node->AddChildLast(ParseDeclaration(true));
		else if( IsAccessDecl() )
			node->AddChildLast(ParseAccessDecl());
		else if( !isStruct && IsClassDefaultStatement() )
		{
			node->AddChildLast(ParseClassDefaultStatement());
		}
		else if( t.type == ttEndStatement )
			// Skip empty declarations
			GetToken(&t);
		else
		{
			Error(TXT_EXPECTED_METHOD_OR_PROPERTY, &t);
			Error(InsteadFound(t), &t);
			return node;
		}

		if( isSyntaxError )
			return node;

		GetToken(&t);
		RewindTo(&t);
	}

	GetToken(&t);
	if( t.type != ttEndStatementBlock )
	{
		Error(ExpectedToken("}"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}
	node->UpdateSourcePos(t.pos, t.length);
	if( sema && recordDecl.IsValid() )
	{
		sema->ActOnRecordFinishAction(recordDecl);
	}
	BindCompletedParsedDeclarationIdentity(sema, node, recordDecl);

	return node;
}

int asCParser::ParseVarInit(asCScriptCode *in_script, asCScriptNode *in_init)
{
	Reset();

	// Tell the parser to validate the identifiers as valid types
	checkValidTypes = true;

	this->script = in_script;
	sourcePos = in_init->tokenPos;

	// If next token is assignment, parse expression
	sToken t;
	GetToken(&t);
	if( t.type == ttAssignment )
	{
		GetToken(&t);
		RewindTo(&t);
		if( t.type == ttStartStatementBlock )
			scriptNode = ParseInitList();
		else
			scriptNode = ParseConditionAsAssignment();
	}
	else if( t.type == ttOpenParanthesis )
	{
		RewindTo(&t);
		scriptNode = ParseArgList();
	}
	else
	{
		int tokens[] = {ttAssignment, ttOpenParanthesis};
		Error(ExpectedOneOf(tokens, 2), &t);
		Error(InsteadFound(t), &t);
	}

	// Don't allow any more tokens after the expression
	GetToken(&t);
	if( t.type != ttEnd && t.type != ttEndStatement && t.type != ttListSeparator && t.type != ttEndStatementBlock )
	{
		asCString msg;
		msg.Format(TXT_UNEXPECTED_TOKEN_s, asCTokenizer::GetDefinition(t.type));
		Error(msg, &t);
	}

	if( isSyntaxError || errorWhileParsing )
		return -1;

	return 0;
}

asCScriptNode *asCParser::SuperficiallyParseVarInit()
{
	asCScriptNode *node = CreateNode(snAssignment);
	if( node == 0 ) return 0;

	sToken t;
	GetToken(&t);
	node->UpdateSourcePos(t.pos, t.length);

	if( t.type == ttAssignment )
	{
		GetToken(&t);
		sToken start = t;

		// Find the end of the expression
		int indentParan = 0;
		int indentBrace = 0;
		while( indentParan || indentBrace || (t.type != ttListSeparator && t.type != ttEndStatement && t.type != ttEndStatementBlock) )
		{
			if( t.type == ttOpenParanthesis )
				indentParan++;
			else if( t.type == ttCloseParanthesis )
				indentParan--;
			else if( t.type == ttStartStatementBlock )
				indentBrace++;
			else if( t.type == ttEndStatementBlock )
				indentBrace--;
			else if( t.type == ttNonTerminatedStringConstant )
			{
				Error(TXT_NONTERMINATED_STRING, &t);
				break;
			}
			else if( t.type == ttEnd )
			{
				Error(TXT_UNEXPECTED_END_OF_FILE, &t);
				Info(TXT_WHILE_PARSING_EXPRESSION, &start);
				break;
			}
			GetToken(&t);
		}

		if( start.length > 0 )
		{
			asCScriptNode* value = CreateNode(snConstant);
			if( value )
			{
				value->SetToken(&start);
				value->UpdateSourcePos(start.pos, start.length);
				node->AddChildLast(value);
			}
		}

		// Rewind so that the next token read is the list separator, end statement, or end statement block
		RewindTo(&t);
	}
	else if( t.type == ttOpenParanthesis )
	{
		sToken start = t;

		// Find the end of the argument list
		int indent = 1;
		while( indent )
		{
			GetToken(&t);
			if( t.type == ttOpenParanthesis )
				indent++;
			else if( t.type == ttCloseParanthesis )
				indent--;
			else if( t.type == ttNonTerminatedStringConstant )
			{
				Error(TXT_NONTERMINATED_STRING, &t);
				break;
			}
			else if( t.type == ttEnd )
			{
				Error(TXT_UNEXPECTED_END_OF_FILE, &t);
				Info(TXT_WHILE_PARSING_ARG_LIST, &start);
				break;
			}
		}
	}
	else
	{
		int tokens[] = {ttAssignment, ttOpenParanthesis};
		Error(ExpectedOneOf(tokens, 2), &t);
		Error(InsteadFound(t), &t);
	}

	return node;
}

asCScriptNode *asCParser::ParseFunctionStatementBlock()
{
	if( sema )
	{
		return ParseStatementBlock();
	}
	return SuperficiallyParseStatementBlock();
}

asCScriptNode *asCParser::SuperficiallyParseStatementBlock()
{
	asCScriptNode *node = CreateNode(snStatementBlock);
	if( node == 0 ) return 0;

	// This function will only superficially parse the statement block in order to find the end of it
	sToken t1;

	GetToken(&t1);
	if( t1.type != ttStartStatementBlock )
	{
		Error(ExpectedToken("{"), &t1);
		Error(InsteadFound(t1), &t1);
		return node;
	}

	node->UpdateSourcePos(t1.pos, t1.length);

	sToken start = t1;

	int level = 1;
	while( level > 0 && !isSyntaxError )
	{
		GetToken(&t1);
		if( t1.type == ttEndStatementBlock )
			level--;
		else if( t1.type == ttStartStatementBlock )
			level++;
		else if( t1.type == ttNonTerminatedStringConstant )
		{
			Error(TXT_NONTERMINATED_STRING, &t1);
			break;
		}
		else if( t1.type == ttEnd )
		{
			Error(TXT_UNEXPECTED_END_OF_FILE, &t1);
			Info(TXT_WHILE_PARSING_STATEMENT_BLOCK, &start);
			break;
		}
	}

	node->UpdateSourcePos(t1.pos, t1.length);

	return node;
}

asCScriptNode* asCParser::SuperficiallyParseStatement()
{
	sToken t1;
	GetToken(&t1);

	if( t1.type == ttStartStatementBlock )
	{
		RewindTo(&t1);
		return SuperficiallyParseStatementBlock();
	}
	else
	{
		asCScriptNode *node = CreateNode(snExpressionStatement);
		if( node == 0 ) return 0;

		node->UpdateSourcePos(t1.pos, t1.length);

		while (t1.type != ttEndStatement && t1.type != ttEndStatementBlock && t1.type != ttEnd)
			GetToken(&t1);

		node->UpdateSourcePos(t1.pos, t1.length);
		return node;
	}
}

// BNF:2: STATBLOCK     ::= '{' {VAR | STATEMENT} '}'
asCScriptNode *asCParser::ParseStatementBlock()
{
	asCScriptNode *node = CreateNode(snStatementBlock);
	if( node == 0 ) return 0;
	asCArray<asASTStmtId> canonicalChildren;
	asCArray<asASTStmtId> canonicalDeclarationSequences;

	sToken t1;

	GetToken(&t1);
	if( t1.type != ttStartStatementBlock )
	{
		Error(ExpectedToken("{"), &t1);
		Error(InsteadFound(t1), &t1);
		return node;
	}

	sToken start = t1;

	node->UpdateSourcePos(t1.pos, t1.length);

	for(;;)
	{
		while( !isSyntaxError )
		{
			GetToken(&t1);
			if( t1.type == ttEndStatementBlock )
			{
				node->UpdateSourcePos(t1.pos, t1.length);
				if( sema )
				{
					asSBlockStatementAction action;
					action.owner = sema->CurrentDeclContext();
					action.children = canonicalChildren;
					action.declarationSequences =
						canonicalDeclarationSequences;
					action.beginOffset = (asUINT)node->tokenPos;
					action.endOffset =
						(asUINT)(node->tokenPos + node->tokenLength);
					action.recovered = false;
					BindParsedStatementIdentity(
						node,
						sema->ActOnBlockStatementAction(action, script));
				}
				return node;
			}
			else
			{
				RewindTo(&t1);

				const bool isDeclaration = IsVarDecl();
				asCScriptNode* const child = isDeclaration
					? ParseDeclaration() : ParseStatement();
				node->AddChildLast(child);
				if( sema && child )
				{
					const asASTStmtId statement =
						FindExactParsedStatementIdentity(child);
					if( statement.IsValid() )
					{
						canonicalChildren.PushLast(statement);
						if( isDeclaration )
						{
							canonicalDeclarationSequences.PushLast(statement);
						}
					}
				}
			}
		}

		if( isSyntaxError )
		{
			// Search for either ';', '{', '}', or end
			GetToken(&t1);
			while( t1.type != ttEndStatement && t1.type != ttEnd &&
				   t1.type != ttStartStatementBlock && t1.type != ttEndStatementBlock )
			{
				GetToken(&t1);
			}

			// Skip this statement block
			if( t1.type == ttStartStatementBlock )
			{
				// Find the end of the block and skip nested blocks
				int level = 1;
				while( level > 0 )
				{
					GetToken(&t1);
					if( t1.type == ttStartStatementBlock ) level++;
					if( t1.type == ttEndStatementBlock ) level--;
					if( t1.type == ttEnd ) break;
				}
			}
			else if( t1.type == ttEndStatementBlock )
			{
				RewindTo(&t1);
			}
			else if( t1.type == ttEnd )
			{
				Error(TXT_UNEXPECTED_END_OF_FILE, &t1);
				Info(TXT_WHILE_PARSING_STATEMENT_BLOCK, &start);
				if( sema && node->tokenLength != 0 )
				{
					asSBlockStatementAction action;
					action.owner = sema->CurrentDeclContext();
					action.children = canonicalChildren;
					action.declarationSequences =
						canonicalDeclarationSequences;
					action.beginOffset = (asUINT)node->tokenPos;
					action.endOffset =
						(asUINT)(node->tokenPos + node->tokenLength);
					action.recovered = true;
					BindParsedStatementIdentity(
						node,
						sema->ActOnBlockStatementAction(action, script));
				}
				return node;
			}

			isSyntaxError = false;
		}
	}
	UNREACHABLE_RETURN;
}

// BNF:4: INITLIST      ::= '{' [ASSIGN | INITLIST] {',' [ASSIGN | INITLIST]} '}'
asCScriptNode *asCParser::ParseInitList()
{
	asCScriptNode *node = CreateNode(snInitList);
	if( node == 0 ) return 0;

	sToken t1;

	GetToken(&t1);
	if( t1.type != ttStartStatementBlock )
	{
		Error(ExpectedToken("{"), &t1);
		Error(InsteadFound(t1), &t1);
		return node;
	}

	node->UpdateSourcePos(t1.pos, t1.length);

	GetToken(&t1);
	if( t1.type == ttEndStatementBlock )
	{
		node->UpdateSourcePos(t1.pos, t1.length);

		// Statement block is finished
		return node;
	}
	else
	{
		RewindTo(&t1);
		for(;;)
		{
			GetToken(&t1);
			if( t1.type == ttListSeparator )
			{
				// No expression
				node->AddChildLast(CreateNode(snUndefined));
				node->lastChild->UpdateSourcePos(t1.pos, 1);

				GetToken(&t1);
				if( t1.type == ttEndStatementBlock )
				{
					// No expression
					node->AddChildLast(CreateNode(snUndefined));
					node->lastChild->UpdateSourcePos(t1.pos, 1);
					node->UpdateSourcePos(t1.pos, t1.length);
					return node;
				}
				RewindTo(&t1);
			}
			else if( t1.type == ttEndStatementBlock )
			{
				// No expression
				node->AddChildLast(CreateNode(snUndefined));
				node->lastChild->UpdateSourcePos(t1.pos, 1);
				node->UpdateSourcePos(t1.pos, t1.length);

				// Statement block is finished
				return node;
			}
			else if( t1.type == ttStartStatementBlock )
			{
				RewindTo(&t1);
				node->AddChildLast(ParseInitList());
				if( isSyntaxError ) return node;

				GetToken(&t1);
				if( t1.type == ttListSeparator )
					continue;
				else if( t1.type == ttEndStatementBlock )
				{
					node->UpdateSourcePos(t1.pos, t1.length);

					// Statement block is finished
					return node;
				}
				else
				{
					Error(ExpectedTokens("}", ","), &t1);
					Error(InsteadFound(t1), &t1);
					return node;
				}
			}
			else
			{
				RewindTo(&t1);
				node->AddChildLast(ParseConditionAsAssignment());
				if( isSyntaxError ) return node;


				GetToken(&t1);
				if( t1.type == ttListSeparator )
					continue;
				else if( t1.type == ttEndStatementBlock )
				{
					node->UpdateSourcePos(t1.pos, t1.length);

					// Statement block is finished
					return node;
				}
				else
				{
					Error(ExpectedTokens("}", ","), &t1);
					Error(InsteadFound(t1), &t1);
					return node;
				}
			}
		}
	}
	UNREACHABLE_RETURN;
}

// BNF:1: VAR           ::= ['private'|'protected'] TYPE IDENTIFIER [( '=' (INITLIST | EXPR)) | ARGLIST] {',' IDENTIFIER [( '=' (INITLIST | EXPR)) | ARGLIST]} ';'
asCScriptNode *asCParser::ParseDeclaration(bool isClassProp, bool isGlobalVar)
{
	asCScriptNode *node = CreateNode(snDeclaration);
	if( node == 0 ) return 0;

	sToken t;
	asDWORD canonicalVariableTraits = 0;
	asCString accessSpecifierName;
	GetToken(&t);
	RewindTo(&t);

	// A class property can be preceeded by private
	if (isClassProp)
	{
		if( t.type == ttPrivate )
		{
			node->AddChildLast(ParseToken(ttPrivate));
			canonicalVariableTraits |= asAST_TRAIT_PRIVATE;
		}
		else if( t.type == ttProtected )
		{
			node->AddChildLast(ParseToken(ttProtected));
			canonicalVariableTraits |= asAST_TRAIT_PROTECTED;
		}
		else if (t.type == ttAccess)
		{
			auto* accessNode = ParseToken(ttAccess);

			sToken topen;
			GetToken(&topen);
			if( topen.type != ttColon )
			{
				Error(ExpectedToken(":"), &topen);
				Error(InsteadFound(topen), &topen);
				return node;
			}

			auto* identifier = ParseIdentifier();
			if (isSyntaxError) return node;
			if( identifier && script && script->code
				&& identifier->tokenLength > 0
				&& identifier->tokenPos + identifier->tokenLength
					<= script->codeLength )
			{
				accessSpecifierName.Assign(
					script->code + identifier->tokenPos,
					identifier->tokenLength);
			}

			accessNode->AddChildLast(identifier);
			node->AddChildLast(accessNode);
		}
	}

	// Parse data type
	asCScriptNode* typeNode = ParseType(true, false, !isClassProp);
	node->AddChildLast(typeNode);
	if( isSyntaxError ) return node;
	asCScriptNode* typeModifierNode = 0;

	// Allow local reference variables
	bool canonicalAutoConstRef = false;
	if (!isClassProp && !isGlobalVar)
	{
		sToken suffix;
		GetToken(&suffix);
		RewindTo(&suffix);

		if (suffix.type == ttAmp)
		{
			typeModifierNode = ParseToken(ttAmp);
			node->AddChildLast(typeModifierNode);
		}

		GetToken(&suffix);
		RewindTo(&suffix);
		if (suffix.type == ttIdentifier && IdentifierIs(suffix, AUTO_CONSTREF_TOKEN))
		{
			canonicalAutoConstRef = true;
			node->AddChildLast(ParseIdentifier());
		}
	}
	asCQualType canonicalVariableType;
	if( sema )
	{
		canonicalVariableType = sema->ActOnQualTypeAction(
			BuildQualTypeSyntaxAction(
				typeNode,
				typeModifierNode), script);
	}
	asCArray<asASTStmtId> canonicalLocalStatements;

	for(;;)
	{
		// Parse identifier
		asCScriptNode* nameNode = ParseIdentifier();
		node->AddChildLast(nameNode);
		if( isSyntaxError ) return node;

		asASTDeclId canonicalVariable;
		if( sema )
		{
			asSVariableHeaderAction action;
			action.type = canonicalVariableType;
			action.traits = canonicalVariableTraits;
			action.accessSpecifierName = accessSpecifierName;
			if( nameNode && script && script->code
				&& nameNode->tokenLength > 0
				&& nameNode->tokenPos + nameNode->tokenLength <= script->codeLength )
			{
				action.name.Assign(
					script->code + nameNode->tokenPos,
					nameNode->tokenLength);
				action.beginOffset = (asUINT)nameNode->tokenPos;
				action.endOffset =
					(asUINT)(nameNode->tokenPos + nameNode->tokenLength);
			}
			canonicalVariable = sema->ActOnVariableHeaderAction(action, script);
			if( canonicalVariable.IsValid() )
			{
				// Identity-only bridge for the transitional Builder producer. The
				// declaration's identity, type, and access are already authoritative.
				sema->BindParsedDeclarationIdentity(
					MakeParseActionIdentity(sema, nameNode), canonicalVariable);
			}
		}

		asCScriptNode* initializerNode = 0;
		if( isClassProp || isGlobalVar )
		{
			GetToken(&t);
			RewindTo(&t);
			if( t.type == ttAssignment || t.type == ttOpenParanthesis )
			{
				// LEGACY keeps Superficial (builder recompiles inits later). CANONICAL Sema
				// must intern the real expression: Superficial stores only the first token
				// as snConstant, so `40 + 1` would evaluate as 40. Globals already parse the
				// full assignment; class properties must too or ctor InitPlan writes 40.
				if( sema && t.type == ttAssignment )
				{
					const sToken assignmentToken = t;
					GetToken(&t);
					GetToken(&t);
					RewindTo(&t);
					asCScriptNode* initializer = 0;
					if( t.type == ttStartStatementBlock )
					{
						initializer = ParseInitList();
					}
					else
					{
						initializer = ParseConditionAsAssignment();
					}
					// Canonical Sema needs the fully parsed RHS now, while the
					// transitional legacy compiler still reparses this node later
					// through ParseVarInit(), which expects its source position to
					// begin at '='. Keep the node's full range anchored there rather
					// than at the first RHS token.
					if( initializer )
					{
						initializer->UpdateSourcePos(
							assignmentToken.pos, assignmentToken.length);
						node->AddChildLast(initializer);
						initializerNode = initializer;
						if( initializer->nodeType == snInitList )
						{
							BindInitListNodeAction(
								initializer, initializer, script, sema,
								canonicalVariableType,
								canonicalVariableType.IsValid(),
								isSyntaxError);
						}
					}
				}
				else if( sema && t.type == ttOpenParanthesis )
				{
					initializerNode = ParseArgList();
					node->AddChildLast(initializerNode);
				}
				else
				{
					initializerNode = SuperficiallyParseVarInit();
					node->AddChildLast(initializerNode);
				}
				if( isSyntaxError ) return node;
			}

			if( sema && canonicalVariable.IsValid() )
			{
				asSVariableInitializerAction action;
				action.variable = canonicalVariable;
				action.beginOffset = (asUINT)nameNode->tokenPos;
				action.endOffset = (asUINT)(
					initializerNode != 0
						? initializerNode->tokenPos + initializerNode->tokenLength
						: nameNode->tokenPos + nameNode->tokenLength);
				action.recovered = isSyntaxError;
				action.autoConstRef = canonicalAutoConstRef;
				if( initializerNode == 0 )
				{
					action.kind = asVARIABLE_INITIALIZER_NONE;
				}
				else if( initializerNode->nodeType == snArgList )
				{
					action.kind = asVARIABLE_INITIALIZER_DIRECT_CONSTRUCT;
					asCArray<asSCallArgumentAction> arguments;
					if( BuildCallArgumentActions(
						initializerNode, script, sema, arguments, isSyntaxError) )
					{
						for( asUINT i = 0; i < arguments.GetLength(); ++i )
						{
							action.arguments.PushLast(arguments[i].expression);
							action.argumentNames.PushLast(arguments[i].name);
						}
					}
					else
					{
						action.kind = asVARIABLE_INITIALIZER_INVALID;
					}
				}
				else
				{
					action.kind = asVARIABLE_INITIALIZER_EXPRESSION;
					action.expression =
						FindSingleExactParsedExpressionIdentity(
							sema, initializerNode);
				}
				if( sema->ActOnVariableInitializerAction(action, script) )
				{
					const asCDecl* const committed =
						sema->GetContext().GetDecl(canonicalVariable);
					if( committed != 0 )
					{
						canonicalVariableType = committed->type;
					}
				}
			}
		}
		else
		{
			// If next token is assignment, parse expression
			GetToken(&t);
			if( t.type == ttOpenParanthesis )
			{
				RewindTo(&t);
				initializerNode = ParseArgList();
				node->AddChildLast(initializerNode);
			}
			else if( t.type == ttAssignment )
			{
				GetToken(&t);
				RewindTo(&t);
				if( t.type == ttStartStatementBlock )
				{
					initializerNode = ParseInitList();
					node->AddChildLast(initializerNode);
					BindInitListNodeAction(
						initializerNode, initializerNode, script, sema,
						canonicalVariableType,
						canonicalVariableType.IsValid(),
						isSyntaxError);
				}
				else
				{
					initializerNode = ParseConditionAsAssignment();
					node->AddChildLast(initializerNode);
				}
			}
			else
				RewindTo(&t);

			if( sema && canonicalVariable.IsValid() )
			{
				asSLocalVariableDeclaratorAction action;
				action.variable = canonicalVariable;
				action.beginOffset = (asUINT)nameNode->tokenPos;
				action.endOffset = (asUINT)(
					initializerNode != 0
						? initializerNode->tokenPos + initializerNode->tokenLength
						: nameNode->tokenPos + nameNode->tokenLength);
				action.recovered = isSyntaxError;
				action.autoConstRef = canonicalAutoConstRef;
				if( initializerNode == 0 )
				{
					action.kind = asVARIABLE_INITIALIZER_NONE;
				}
				else if( initializerNode->nodeType == snArgList )
				{
					action.kind = asVARIABLE_INITIALIZER_DIRECT_CONSTRUCT;
					asCArray<asSCallArgumentAction> arguments;
					if( BuildCallArgumentActions(
						initializerNode, script, sema, arguments, isSyntaxError) )
					{
						for( asUINT i = 0; i < arguments.GetLength(); ++i )
						{
							action.arguments.PushLast(arguments[i].expression);
							action.argumentNames.PushLast(arguments[i].name);
						}
					}
					else
					{
						action.kind = asVARIABLE_INITIALIZER_INVALID;
					}
				}
				else
				{
					action.kind = asVARIABLE_INITIALIZER_EXPRESSION;
					action.expression =
						FindSingleExactParsedExpressionIdentity(
							sema, initializerNode);
				}
				if( sema->ActOnLocalVariableDeclaratorAction(
					action, script, canonicalLocalStatements) )
				{
					const asCDecl* const committed =
						sema->GetContext().GetDecl(canonicalVariable);
					if( committed != 0 )
					{
						canonicalVariableType = committed->type;
					}
				}
			}
			// A bounded initializer action is meaningful even when its closing
			// delimiter is missing: preserve the completed prefix for diagnostics
			// and recovery without replaying the declaration shell.
			if( isSyntaxError ) return node;
		}

		// continue if list separator, else terminate with end statement
		GetToken(&t);
		if( t.type == ttListSeparator )
			continue;
		else if( t.type == ttEndStatement )
		{
			node->UpdateSourcePos(t.pos, t.length);
			if( sema && !isClassProp && !isGlobalVar )
			{
				BindParsedStatementIdentity(
					node,
					sema->ActOnLocalVariableDeclarationAction(
						canonicalLocalStatements, script,
						(asUINT)node->tokenPos,
						(asUINT)(node->tokenPos + node->tokenLength)));
			}

			return node;
		}
		else
		{
			Error(ExpectedTokens(",", ";"), &t);
			Error(InsteadFound(t), &t);
			return node;
		}
	}
	UNREACHABLE_RETURN;
}

// BNF:7: STATEMENT     ::= (IF | FOR | WHILE | RETURN | STATBLOCK | BREAK | CONTINUE | DOWHILE | SWITCH | EXPRSTAT)
asCScriptNode *asCParser::ParseStatement()
{
	sToken t1;

	GetToken(&t1);
	RewindTo(&t1);

	asCScriptNode* parsed = 0;
	if( t1.type == ttIf )
		parsed = ParseIf();
	else if( t1.type == ttFor )
		parsed = ParseFor();
	else if( t1.type == ttForeach )
		parsed = ParseForeach();
	else if( t1.type == ttWhile )
		parsed = ParseWhile();
	else if( t1.type == ttReturn )
		parsed = ParseReturn();
	else if( t1.type == ttStartStatementBlock )
		parsed = ParseStatementBlock();
	else if( t1.type == ttBreak )
		parsed = ParseBreak();
	else if( t1.type == ttContinue )
		parsed = ParseContinue();
	else if( t1.type == ttFallthrough )
		parsed = ParseFallthrough();
	else if( t1.type == ttDo )
		parsed = ParseDoWhile();
	else if( t1.type == ttSwitch )
		parsed = ParseSwitch();
	else
	{
		if( IsVarDecl() )
		{
			Error(TXT_UNEXPECTED_VAR_DECL, &t1);
			return 0;
		}
		parsed = ParseExpressionStatement();
	}

	return parsed;
}

// BNF:8: EXPRSTAT      ::= [ASSIGN] ';'
asCScriptNode *asCParser::ParseExpressionStatement()
{
	asCScriptNode *node = CreateNode(snExpressionStatement);
	if( node == 0 ) return 0;

	sToken t;
	GetToken(&t);
	if( t.type == ttEndStatement )
	{
		node->UpdateSourcePos(t.pos, t.length);
		BindParsedStatementIdentity(
			node,
			ActOnLeafStatementAction(
				sema, script, node, asLEAF_STATEMENT_EXPRESSION,
				asASTExprId(), false));
		return node;
	}

	RewindTo(&t);

	node->AddChildLast(ParseAssignment());
	if( isSyntaxError ) return node;

	GetToken(&t);
	if( t.type != ttEndStatement )
	{
		Error(ExpectedToken(";"), &t);
		Error(InsteadFound(t), &t);
		BindParsedStatementIdentity(
			node,
			ActOnLeafStatementAction(
				sema, script, node, asLEAF_STATEMENT_EXPRESSION,
				FindSingleExactParsedExpressionIdentity(sema, node->firstChild),
				true));
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);
	BindParsedStatementIdentity(
		node,
		ActOnLeafStatementAction(
			sema, script, node, asLEAF_STATEMENT_EXPRESSION,
			FindSingleExactParsedExpressionIdentity(sema, node->firstChild),
			false));

	return node;
}

// BNF:8: EXPRSTAT      ::= [ASSIGN] ';'
asCScriptNode *asCParser::ParseExpressionStatementCondition()
{
	asCScriptNode *node = CreateNode(snExpressionStatement);
	if( node == 0 ) return 0;

	sToken t;
	GetToken(&t);
	if( t.type == ttEndStatement )
	{
		node->UpdateSourcePos(t.pos, t.length);

		return node;
	}

	RewindTo(&t);

	node->AddChildLast(ParseCondition());
	if( isSyntaxError ) return node;

	GetToken(&t);
	if( t.type != ttEndStatement )
	{
		Error(ExpectedToken(";"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);

	return node;
}

// BNF:8: SWITCH        ::= 'switch' '(' ASSIGN ')' '{' {CASE} '}'
asCScriptNode *asCParser::ParseSwitch()
{
	asCScriptNode *node = CreateNode(snSwitch);
	if( node == 0 ) return 0;
	asCScriptNode* selectorNode = 0;
	asCArray<asASTStmtId> canonicalCases;
	asASTStmtId canonicalControl;
	auto publishTypedSwitch = [&](bool recovered)
	{
		if( sema == 0 )
		{
			return;
		}
		asSSwitchStatementAction action;
		action.control = canonicalControl;
		action.owner = sema->CurrentDeclContext();
		action.selector = FindSingleExactParsedExpressionIdentity(
			sema, selectorNode);
		action.cases = canonicalCases;
		action.beginOffset = (asUINT)node->tokenPos;
		action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
		action.recovered = recovered;
		BindParsedStatementIdentity(
			node, sema->ActOnSwitchStatementAction(action, script));
	};

	sToken t;
	GetToken(&t);
	if( t.type != ttSwitch )
	{
		Error(ExpectedToken("switch"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);

	GetToken(&t);
	if( t.type != ttOpenParanthesis )
	{
		Error(ExpectedToken("("), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	selectorNode = ParseCondition();
	node->AddChildLast(selectorNode);
	if( isSyntaxError ) return node;

	GetToken(&t);
	if( t.type != ttCloseParanthesis )
	{
		Error(ExpectedToken(")"), &t);
		Error(InsteadFound(t), &t);
		publishTypedSwitch(true);
		return node;
	}

	if( sema )
	{
		asSControlStatementHeaderAction header;
		header.kind = asCONTROL_STATEMENT_SWITCH;
		header.owner = sema->CurrentDeclContext();
		header.beginOffset = (asUINT)node->tokenPos;
		header.endOffset = (asUINT)(t.pos + t.length);
		canonicalControl = sema->ActOnControlStatementHeaderAction(
			header, script);
	}

	GetToken(&t);
	if( t.type != ttStartStatementBlock )
	{
		Error(ExpectedToken("{"), &t);
		Error(InsteadFound(t), &t);
		publishTypedSwitch(true);
		return node;
	}

	while( !isSyntaxError )
	{
		GetToken(&t);

		if( t.type == ttEndStatementBlock )
			break;

		RewindTo(&t);

		if( t.type != ttCase && t.type != ttDefault )
		{
			const char *tokens[] = {"case", "default"};
			Error(ExpectedOneOf(tokens, 2), &t);
			Error(InsteadFound(t), &t);
			publishTypedSwitch(true);
			return node;
		}

		asCScriptNode* const caseNode = ParseCase();
		node->AddChildLast(caseNode);
		const asASTStmtId caseStatement =
			FindExactParsedStatementIdentity(caseNode);
		if( caseStatement.IsValid() )
		{
			canonicalCases.PushLast(caseStatement);
		}
		if( isSyntaxError )
		{
			publishTypedSwitch(true);
			return node;
		}
	}

	if( t.type != ttEndStatementBlock )
	{
		Error(ExpectedToken("}"), &t);
		Error(InsteadFound(t), &t);
		publishTypedSwitch(true);
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);
	publishTypedSwitch(false);
	return node;
}

asCScriptNode *asCParser::ParseFallthrough()
{
	asCScriptNode *node = CreateNode(snFallthrough);
	if( node == 0 ) return 0;

	sToken t;
	GetToken(&t);
	if( t.type != ttFallthrough )
	{
		Error(ExpectedToken("fallthrough"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);

	GetToken(&t);
	if( t.type != ttEndStatement )
	{
		Error(ExpectedToken(";"), &t);
		Error(InsteadFound(t), &t);
		BindParsedStatementIdentity(
			node,
			ActOnLeafStatementAction(
				sema, script, node, asLEAF_STATEMENT_FALLTHROUGH,
				asASTExprId(), true));
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);
	BindParsedStatementIdentity(
		node,
		ActOnLeafStatementAction(
			sema, script, node, asLEAF_STATEMENT_FALLTHROUGH,
			asASTExprId(), false));

	return node;
}

// BNF:9: CASE          ::= (('case' EXPR) | 'default') ':' {STATEMENT}
asCScriptNode *asCParser::ParseCase()
{
	asCScriptNode *node = CreateNode(snCase);
	if( node == 0 ) return 0;
	asCScriptNode* valueNode = 0;
	asCArray<asASTStmtId> canonicalChildren;
	asECaseStatementActionKind canonicalKind = asCASE_STATEMENT_INVALID;
	auto publishTypedCase = [&](bool recovered)
	{
		if( sema == 0 )
		{
			return;
		}
		asSCaseStatementAction action;
		action.kind = canonicalKind;
		action.switchStatement = sema->CurrentSwitchControl();
		action.owner = sema->CurrentDeclContext();
		action.value = FindSingleExactParsedExpressionIdentity(sema, valueNode);
		action.children = canonicalChildren;
		action.beginOffset = (asUINT)node->tokenPos;
		action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
		action.recovered = recovered;
		BindParsedStatementIdentity(
			node, sema->ActOnCaseStatementAction(action, script));
	};

	sToken t;
	GetToken(&t);
	if( t.type != ttCase && t.type != ttDefault )
	{
		Error(ExpectedTokens("case", "default"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);

	if(t.type == ttCase)
	{
		canonicalKind = asCASE_STATEMENT_CASE;
		valueNode = ParseExpression();
		node->AddChildLast(valueNode);
	}
	else
	{
		canonicalKind = asCASE_STATEMENT_DEFAULT;
	}

	GetToken(&t);
	if( t.type != ttColon )
	{
		Error(ExpectedToken(":"), &t);
		Error(InsteadFound(t), &t);
		publishTypedCase(true);
		return node;
	}
	node->UpdateSourcePos(t.pos, t.length);

	// Parse statements until we find either of }, case, default, break, and fallthrough
	GetToken(&t);
	RewindTo(&t);
	while( t.type != ttCase &&
		   t.type != ttDefault &&
		   t.type != ttEndStatementBlock &&
		   t.type != ttBreak &&
		   t.type != ttFallthrough )
	{
		asCScriptNode* child = 0;
		if( IsVarDecl() )
			// Variable declarations are not allowed, but we parse it anyway to give a good error message
			child = ParseDeclaration();
		else
			child = ParseStatement();
		node->AddChildLast(child);
		const asASTStmtId childStatement =
			FindExactParsedStatementIdentity(child);
		if( childStatement.IsValid() )
		{
			canonicalChildren.PushLast(childStatement);
		}
		if( isSyntaxError )
		{
			publishTypedCase(true);
			return node;
		}

		GetToken(&t);
		RewindTo(&t);
	}

	// If the case was ended with a break statement, add it to the node
	asCScriptNode* terminal = 0;
	if( t.type == ttBreak )
		terminal = ParseBreak();
	else if( t.type == ttFallthrough )
		terminal = ParseFallthrough();
	if( terminal != 0 )
	{
		node->AddChildLast(terminal);
		const asASTStmtId terminalStatement =
			FindExactParsedStatementIdentity(terminal);
		if( terminalStatement.IsValid() )
		{
			canonicalChildren.PushLast(terminalStatement);
		}
		if( isSyntaxError )
		{
			publishTypedCase(true);
			return node;
		}
	}
	publishTypedCase(false);

	return node;
}

// BNF:8: IF            ::= 'if' '(' ASSIGN ')' STATEMENT ['else' STATEMENT]
asCScriptNode *asCParser::ParseIf()
{
	asCScriptNode *node = CreateNode(snIf);
	if( node == 0 ) return 0;
	auto publishTypedIf = [&](bool recovered)
	{
		if( sema == 0 )
		{
			return;
		}
		asSIfStatementAction action;
		action.owner = sema->CurrentDeclContext();
		action.condition = FindSingleExactParsedExpressionIdentity(
			sema, node->firstChild);
		const asCScriptNode* const thenNode =
			node->firstChild ? node->firstChild->next : 0;
		const asCScriptNode* const elseNode = thenNode ? thenNode->next : 0;
		action.thenStatement = FindExactParsedStatementIdentity(thenNode);
		action.elseStatement = FindExactParsedStatementIdentity(elseNode);
		action.beginOffset = (asUINT)node->tokenPos;
		action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
		action.recovered = recovered;
		BindParsedStatementIdentity(
			node,
			sema->ActOnIfStatementAction(action, script));
	};

	sToken t;
	GetToken(&t);
	if( t.type != ttIf )
	{
		Error(ExpectedToken("if"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);

	GetToken(&t);
	if( t.type != ttOpenParanthesis )
	{
		Error(ExpectedToken("("), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->AddChildLast(ParseCondition());
	if( isSyntaxError ) return node;

	GetToken(&t);
	if( t.type != ttCloseParanthesis )
	{
		Error(ExpectedToken(")"), &t);
		Error(InsteadFound(t), &t);
		publishTypedIf(true);
		return node;
	}

	node->AddChildLast(ParseStatement());
	if( isSyntaxError )
	{
		publishTypedIf(true);
		return node;
	}

	GetToken(&t);
	if( t.type != ttElse )
	{
		// No else statement return already
		RewindTo(&t);
		publishTypedIf(false);
		return node;
	}

	node->AddChildLast(ParseStatement());
	if( isSyntaxError )
	{
		publishTypedIf(true);
		return node;
	}
	publishTypedIf(false);
	return node;
}

// BNF:8: FOR           ::= 'for' '(' (VAR | EXPRSTAT) EXPRSTAT [ASSIGN {',' ASSIGN}] ')' STATEMENT
asCScriptNode *asCParser::ParseFor()
{
	asCScriptNode *node = CreateNode(snFor);
	if( node == 0 ) return 0;
	asCScriptNode* initNode = 0;
	asCScriptNode* conditionNode = 0;
	asCScriptNode* bodyNode = 0;
	asCArray<asASTExprId> canonicalIncrements;
	asASTStmtId canonicalControl;
	auto publishTypedFor = [&](bool recovered)
	{
		if( sema == 0 )
		{
			return;
		}
		asSForStatementAction action;
		action.control = canonicalControl;
		action.owner = sema->CurrentDeclContext();
		action.initStatement = FindExactParsedStatementIdentity(initNode);
		action.condition = FindSingleExactParsedExpressionIdentity(
			sema, conditionNode);
		action.increments = canonicalIncrements;
		action.body = FindExactParsedStatementIdentity(bodyNode);
		action.beginOffset = (asUINT)node->tokenPos;
		action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
		action.recovered = recovered;
		BindParsedStatementIdentity(
			node, sema->ActOnForStatementAction(action, script));
	};

	sToken t;
	GetToken(&t);
	if( t.type != ttFor )
	{
		Error(ExpectedToken("for"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);

	GetToken(&t);
	if( t.type != ttOpenParanthesis )
	{
		Error(ExpectedToken("("), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	if( IsVarDecl() )
	{
		initNode = ParseDeclaration();
	}
	else
		initNode = ParseExpressionStatement();
	node->AddChildLast(initNode);
	if( isSyntaxError )
	{
		publishTypedFor(true);
		return node;
	}

	conditionNode = ParseExpressionStatementCondition();
	node->AddChildLast(conditionNode);
	if( isSyntaxError )
	{
		publishTypedFor(true);
		return node;
	}

	GetToken(&t);
	if( t.type != ttCloseParanthesis )
	{
		RewindTo(&t);

		// Parse N increment statements separated by ,
		for(;;)
		{
			asCScriptNode *n = CreateNode(snExpressionStatement);
			if( n == 0 ) return 0;
			node->AddChildLast(n);
			asCScriptNode* const incrementNode = ParseAssignment();
			n->AddChildLast(incrementNode);
			const asASTExprId increment =
				FindSingleExactParsedExpressionIdentity(sema, incrementNode);
			if( increment.IsValid() )
			{
				canonicalIncrements.PushLast(increment);
			}
			if( isSyntaxError )
			{
				publishTypedFor(true);
				return node;
			}

			GetToken(&t);
			if( t.type == ttListSeparator )
				continue;
			else if( t.type == ttCloseParanthesis )
				break;
			else
			{
				const char *tokens[] = {",", ")"};
				Error(ExpectedOneOf(tokens, 2), &t);
				Error(InsteadFound(t), &t);
				publishTypedFor(true);
				return node;
			}
		}
	}

	if( sema )
	{
		asSControlStatementHeaderAction header;
		header.kind = asCONTROL_STATEMENT_FOR;
		header.owner = sema->CurrentDeclContext();
		header.beginOffset = (asUINT)node->tokenPos;
		header.endOffset = (asUINT)(t.pos + t.length);
		canonicalControl = sema->ActOnControlStatementHeaderAction(
			header, script);
	}

	bodyNode = ParseStatement();
	node->AddChildLast(bodyNode);
	if( isSyntaxError )
	{
		publishTypedFor(true);
		return node;
	}
	publishTypedFor(false);
	return node;
}

//[UE++]: Add stock foreach(...) parsing support for APV2.
asCScriptNode *asCParser::ParseForeachVariable()
{
	asCScriptNode *node = CreateNode(snDeclaration);
	if( node == 0 ) return 0;

	sToken start;
	GetToken(&start);
	RewindTo(&start);

	asCScriptNode* const typeNode = ParseType(true, false, true);
	node->AddChildLast(typeNode);
	if( isSyntaxError ) return node;

	sToken suffix;
	asCScriptNode* typeModifierNode = 0;
	GetToken(&suffix);
	RewindTo(&suffix);
	if( suffix.type == ttAmp )
	{
		typeModifierNode = ParseToken(ttAmp);
		node->AddChildLast(typeModifierNode);
	}

	GetToken(&suffix);
	RewindTo(&suffix);
	if( suffix.type == ttIdentifier && IdentifierIs(suffix, AUTO_CONSTREF_TOKEN) )
	{
		node->AddChildLast(ParseIdentifier());
	}

	GetToken(&suffix);
	RewindTo(&suffix);
	if( suffix.type != ttIdentifier )
	{
		Error(TXT_FOREACH_VARIABLE_DECLARATION, &suffix);
		Error(InsteadFound(suffix), &suffix);
		return node;
	}

	asCScriptNode* const nameNode = ParseIdentifier();
	node->AddChildLast(nameNode);
	if( isSyntaxError ) return node;

	node->UpdateSourcePos(start.pos, (node->lastChild->tokenPos + node->lastChild->tokenLength) - start.pos);
	if( sema )
	{
		asSVariableHeaderAction action;
		action.type = sema->ActOnQualTypeAction(
			BuildQualTypeSyntaxAction(
				typeNode,
				typeModifierNode), script);
		if( nameNode != 0 && script != 0 && script->code != 0
			&& nameNode->tokenLength > 0
			&& nameNode->tokenPos + nameNode->tokenLength <= script->codeLength )
		{
			action.name.Assign(
				script->code + nameNode->tokenPos,
				nameNode->tokenLength);
			action.beginOffset = (asUINT)nameNode->tokenPos;
			action.endOffset = (asUINT)(
				nameNode->tokenPos + nameNode->tokenLength);
		}
		const asASTDeclId variable =
			sema->ActOnVariableHeaderAction(action, script);
		if( variable.IsValid() )
		{
			sema->BindParsedDeclarationIdentity(
				MakeParseActionIdentity(sema, nameNode), variable);
			BindParsedStatementIdentity(
				node,
				sema->ActOnForeachVariableAction(
					variable, script,
					(asUINT)node->tokenPos,
					(asUINT)(node->tokenPos + node->tokenLength)));
		}
	}
	return node;
}

asCScriptNode *asCParser::ParseForeach()
{
	asCScriptNode *node = CreateNode(snForEach);
	if( node == 0 ) return 0;
	asCScriptNode* valueNode = 0;
	asCScriptNode* keyNode = 0;
	asCScriptNode* rangeNode = 0;
	asCScriptNode* bodyNode = 0;
	asASTStmtId canonicalControl;
	auto publishTypedForeach = [&](bool recovered)
	{
		if( sema == 0 )
		{
			return;
		}
		asSForeachStatementAction action;
		action.control = canonicalControl;
		action.owner = sema->CurrentDeclContext();
		action.valueStatement = FindExactParsedStatementIdentity(valueNode);
		action.keyStatement = FindExactParsedStatementIdentity(keyNode);
		action.rangeExpression = FindSingleExactParsedExpressionIdentity(
			sema, rangeNode);
		action.body = FindExactParsedStatementIdentity(bodyNode);
		action.beginOffset = (asUINT)node->tokenPos;
		action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
		action.recovered = recovered;
		BindParsedStatementIdentity(
			node, sema->ActOnForeachStatementAction(action, script));
	};

	sToken t;
	GetToken(&t);
	if( t.type != ttForeach )
	{
		Error(ExpectedToken("foreach"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);
	sToken start = t;

	GetToken(&t);
	if( t.type != ttOpenParanthesis )
	{
		Error(ExpectedToken("("), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	valueNode = ParseForeachVariable();
	node->AddChildLast(valueNode);
	if( isSyntaxError )
	{
		Info(TXT_WHILE_PARSING_FOREACH, &start);
		publishTypedForeach(true);
		return node;
	}

	GetToken(&t);
	if( t.type == ttListSeparator )
	{
		keyNode = ParseForeachVariable();
		node->AddChildLast(keyNode);
		if( isSyntaxError )
		{
			Info(TXT_WHILE_PARSING_FOREACH, &start);
			publishTypedForeach(true);
			return node;
		}

		GetToken(&t);
	}

	if( t.type != ttColon )
	{
		Error(ExpectedToken(":"), &t);
		Error(InsteadFound(t), &t);
		Info(TXT_WHILE_PARSING_FOREACH, &start);
		publishTypedForeach(true);
		return node;
	}

	rangeNode = ParseCondition();
	node->AddChildLast(rangeNode);
	if( isSyntaxError )
	{
		Info(TXT_WHILE_PARSING_FOREACH, &start);
		publishTypedForeach(true);
		return node;
	}

	GetToken(&t);
	if( t.type != ttCloseParanthesis )
	{
		Error(ExpectedToken(")"), &t);
		Error(InsteadFound(t), &t);
		Info(TXT_WHILE_PARSING_FOREACH, &start);
		publishTypedForeach(true);
		return node;
	}

	if( sema )
	{
		asSControlStatementHeaderAction header;
		header.kind = asCONTROL_STATEMENT_FOREACH;
		header.owner = sema->CurrentDeclContext();
		header.beginOffset = (asUINT)node->tokenPos;
		header.endOffset = (asUINT)(t.pos + t.length);
		canonicalControl = sema->ActOnControlStatementHeaderAction(
			header, script);
	}

	bodyNode = ParseStatement();
	node->AddChildLast(bodyNode);
	if( isSyntaxError )
	{
		Info(TXT_WHILE_PARSING_FOREACH, &start);
		publishTypedForeach(true);
		return node;
	}

	node->UpdateSourcePos(start.pos, (node->lastChild->tokenPos + node->lastChild->tokenLength) - start.pos);
	publishTypedForeach(false);
	return node;
}
//[UE--]

// BNF:8: WHILE         ::= 'while' '(' ASSIGN ')' STATEMENT
asCScriptNode *asCParser::ParseWhile()
{
	asCScriptNode *node = CreateNode(snWhile);
	if( node == 0 ) return 0;
	asASTStmtId canonicalControl;
	auto publishTypedWhile = [&](bool recovered)
	{
		if( sema == 0 )
		{
			return;
		}
		asSWhileStatementAction action;
		action.kind = asWHILE_STATEMENT_WHILE;
		action.control = canonicalControl;
		action.owner = sema->CurrentDeclContext();
		action.condition = FindSingleExactParsedExpressionIdentity(
			sema, node->firstChild);
		action.body = FindExactParsedStatementIdentity(
			node->firstChild ? node->firstChild->next : 0);
		action.beginOffset = (asUINT)node->tokenPos;
		action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
		action.recovered = recovered;
		BindParsedStatementIdentity(
			node, sema->ActOnWhileStatementAction(action, script));
	};

	sToken t;
	GetToken(&t);
	if( t.type != ttWhile )
	{
		Error(ExpectedToken("while"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);

	GetToken(&t);
	if( t.type != ttOpenParanthesis )
	{
		Error(ExpectedToken("("), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->AddChildLast(ParseCondition());
	if( isSyntaxError ) return node;

	GetToken(&t);
	if( t.type != ttCloseParanthesis )
	{
		Error(ExpectedToken(")"), &t);
		Error(InsteadFound(t), &t);
		publishTypedWhile(true);
		return node;
	}

	if( sema )
	{
		asSControlStatementHeaderAction action;
		action.kind = asCONTROL_STATEMENT_WHILE;
		action.owner = sema->CurrentDeclContext();
		action.beginOffset = (asUINT)node->tokenPos;
		action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
		canonicalControl =
			sema->ActOnControlStatementHeaderAction(action, script);
	}

	node->AddChildLast(ParseStatement());
	if( isSyntaxError )
	{
		publishTypedWhile(true);
		return node;
	}
	publishTypedWhile(false);
	return node;
}

// BNF:8: DOWHILE       ::= 'do' STATEMENT 'while' '(' ASSIGN ')' ';'
asCScriptNode *asCParser::ParseDoWhile()
{
	asCScriptNode *node = CreateNode(snDoWhile);
	if( node == 0 ) return 0;
	asASTStmtId canonicalControl;
	auto publishTypedDoWhile = [&](bool recovered)
	{
		if( sema == 0 )
		{
			return;
		}
		asSWhileStatementAction action;
		action.kind = asWHILE_STATEMENT_DO_WHILE;
		action.control = canonicalControl;
		action.owner = sema->CurrentDeclContext();
		action.body = FindExactParsedStatementIdentity(node->firstChild);
		action.condition = FindSingleExactParsedExpressionIdentity(
			sema, node->firstChild ? node->firstChild->next : 0);
		action.beginOffset = (asUINT)node->tokenPos;
		action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
		action.recovered = recovered;
		BindParsedStatementIdentity(
			node, sema->ActOnWhileStatementAction(action, script));
	};

	sToken t;
	GetToken(&t);
	if( t.type != ttDo )
	{
		Error(ExpectedToken("do"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);

	if( sema )
	{
		asSControlStatementHeaderAction action;
		action.kind = asCONTROL_STATEMENT_DO_WHILE;
		action.owner = sema->CurrentDeclContext();
		action.beginOffset = (asUINT)node->tokenPos;
		action.endOffset = (asUINT)(node->tokenPos + node->tokenLength);
		canonicalControl =
			sema->ActOnControlStatementHeaderAction(action, script);
	}

	node->AddChildLast(ParseStatement());
	if( isSyntaxError )
	{
		publishTypedDoWhile(true);
		return node;
	}

	GetToken(&t);
	if( t.type != ttWhile )
	{
		Error(ExpectedToken("while"), &t);
		Error(InsteadFound(t), &t);
		publishTypedDoWhile(true);
		return node;
	}

	GetToken(&t);
	if( t.type != ttOpenParanthesis )
	{
		Error(ExpectedToken("("), &t);
		Error(InsteadFound(t), &t);
		publishTypedDoWhile(true);
		return node;
	}

	node->AddChildLast(ParseCondition());
	if( isSyntaxError )
	{
		publishTypedDoWhile(true);
		return node;
	}

	GetToken(&t);
	if( t.type != ttCloseParanthesis )
	{
		Error(ExpectedToken(")"), &t);
		Error(InsteadFound(t), &t);
		publishTypedDoWhile(true);
		return node;
	}

	GetToken(&t);
	if( t.type != ttEndStatement )
	{
		Error(ExpectedToken(";"), &t);
		Error(InsteadFound(t), &t);
		publishTypedDoWhile(true);
		return node;
	}
	node->UpdateSourcePos(t.pos, t.length);
	publishTypedDoWhile(false);
	return node;
}

// BNF:8: RETURN        ::= 'return' [ASSIGN] ';'
asCScriptNode *asCParser::ParseReturn()
{
	asCScriptNode *node = CreateNode(snReturn);
	if( node == 0 ) return 0;

	sToken t;
	GetToken(&t);
	if( t.type != ttReturn )
	{
		Error(ExpectedToken("return"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);

	GetToken(&t);
	if( t.type == ttEndStatement )
	{
		node->UpdateSourcePos(t.pos, t.length);
		BindParsedStatementIdentity(
			node,
			ActOnLeafStatementAction(
				sema, script, node, asLEAF_STATEMENT_RETURN,
				asASTExprId(), false));
		return node;
	}

	RewindTo(&t);

	node->AddChildLast(ParseCondition());
	if( isSyntaxError ) return node;

	GetToken(&t);
	if( t.type != ttEndStatement )
	{
		Error(ExpectedToken(";"), &t);
		Error(InsteadFound(t), &t);
		BindParsedStatementIdentity(
			node,
			ActOnLeafStatementAction(
				sema, script, node, asLEAF_STATEMENT_RETURN,
				FindSingleExactParsedExpressionIdentity(sema, node->firstChild),
				true));
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);
	BindParsedStatementIdentity(
		node,
		ActOnLeafStatementAction(
			sema, script, node, asLEAF_STATEMENT_RETURN,
			FindSingleExactParsedExpressionIdentity(sema, node->firstChild),
			false));

	return node;
}

// BNF:8: BREAK         ::= 'break' ';'
asCScriptNode *asCParser::ParseBreak()
{
	asCScriptNode *node = CreateNode(snBreak);
	if( node == 0 ) return 0;

	sToken t;
	GetToken(&t);
	if( t.type != ttBreak )
	{
		Error(ExpectedToken("break"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);

	GetToken(&t);
	if( t.type != ttEndStatement )
	{
		Error(ExpectedToken(";"), &t);
		Error(InsteadFound(t), &t);
		BindParsedStatementIdentity(
			node,
			ActOnLeafStatementAction(
				sema, script, node, asLEAF_STATEMENT_BREAK,
				asASTExprId(), true));
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);
	BindParsedStatementIdentity(
		node,
		ActOnLeafStatementAction(
			sema, script, node, asLEAF_STATEMENT_BREAK,
			asASTExprId(), false));

	return node;
}

// BNF:8: CONTINUE      ::= 'continue' ';'
asCScriptNode *asCParser::ParseContinue()
{
	asCScriptNode *node = CreateNode(snContinue);
	if( node == 0 ) return 0;

	sToken t;
	GetToken(&t);
	if( t.type != ttContinue )
	{
		Error(ExpectedToken("continue"), &t);
		Error(InsteadFound(t), &t);
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);

	GetToken(&t);
	if( t.type != ttEndStatement )
	{
		Error(ExpectedToken(";"), &t);
		Error(InsteadFound(t), &t);
		BindParsedStatementIdentity(
			node,
			ActOnLeafStatementAction(
				sema, script, node, asLEAF_STATEMENT_CONTINUE,
				asASTExprId(), true));
		return node;
	}

	node->UpdateSourcePos(t.pos, t.length);
	BindParsedStatementIdentity(
		node,
		ActOnLeafStatementAction(
			sema, script, node, asLEAF_STATEMENT_CONTINUE,
			asASTExprId(), false));

	return node;
}

// TODO: typedef: Typedefs should accept complex types as well
// BNF:1: TYPEDEF       ::= 'typedef' PRIMTYPE IDENTIFIER ';'
asCScriptNode *asCParser::ParseTypedef()
{
	// Create the typedef node
	asCScriptNode *node = CreateNode(snTypedef);
	if( node == 0 ) return 0;

	sToken	token;

	GetToken(&token);
	if( token.type != ttTypedef)
	{
		Error(ExpectedToken(asCTokenizer::GetDefinition(ttTypedef)), &token);
		Error(InsteadFound(token), &token);
		return node;
	}

	node->SetToken(&token);
	node->UpdateSourcePos(token.pos, token.length);

	// Parse the base type
	GetToken(&token);
	RewindTo(&token);

	// Make sure it is a primitive type (except ttVoid)
	if( !IsRealType(token.type) || token.type == ttVoid )
	{
		asCString str;
		str.Format(TXT_UNEXPECTED_TOKEN_s, asCTokenizer::GetDefinition(token.type));
		Error(str, &token);
		return node;
	}

	asCScriptNode* const typeNode = ParseRealType();
	node->AddChildLast(typeNode);
	asCScriptNode* const identNode = ParseIdentifier();
	node->AddChildLast(identNode);
	if( isSyntaxError ) return node;

	// Clang-shaped: publish only the recognized alias identity, the Parser-
	// resolved primitive token and exact name range after the identifier but
	// before ';'. Keep snTypedef solely for the explicit LEGACY oracle.
	if( sema )
	{
		asSTypedefDeclAction action;
		if( identNode && script && script->code
			&& identNode->tokenLength > 0
			&& identNode->tokenPos + identNode->tokenLength <= script->codeLength )
		{
			action.name.Assign(
				script->code + identNode->tokenPos,
				(asUINT)identNode->tokenLength);
			action.beginOffset = (asUINT)identNode->tokenPos;
			action.endOffset =
				(asUINT)(identNode->tokenPos + identNode->tokenLength);
		}
		action.primitiveToken = typeNode
			? typeNode->tokenType : ttUnrecognizedToken;
		sema->ActOnTypedefAction(action, script);
	}

	// Check for the end of the typedef
	GetToken(&token);
	if( token.type != ttEndStatement )
	{
		RewindTo(&token);
		Error(ExpectedToken(asCTokenizer::GetDefinition(token.type)), &token);
		Error(InsteadFound(token), &token);
	}

	return node;
}

void asCParser::ParseMethodAttributes(asCScriptNode* funcNode, asDWORD* canonicalTraits)
{
	sToken t1;

	for (;;)
	{
		GetToken(&t1);
		RewindTo(&t1);

		if (IdentifierIs(t1, PROPERTY_TOKEN))
		{
			Error(TXT_PROPERTY_DECORATOR_REMOVED, &t1);
			return;
		}

		if (IdentifierIs(t1, FINAL_TOKEN)
			|| IdentifierIs(t1, OVERRIDE_TOKEN)
			|| IdentifierIs(t1, MIXIN_TOKEN)
			|| IdentifierIs(t1, ACCEPT_TEMPORARY_TOKEN)
			|| IdentifierIs(t1, EXTERNAL_IMPLICIT_THIS_TOKEN)
			|| IdentifierIs(t1, NO_DISCARD_TOKEN)
			|| IdentifierIs(t1, ALLOW_DISCARD_TOKEN)
			|| IdentifierIs(t1, GENERATED_FUNCTION_TOKEN)
			|| IdentifierIs(t1, DEPRECATED_TOKEN)
			|| IdentifierIs(t1, DEFAULTS_TOKEN)
			|| IdentifierIs(t1, UNSAFE_DURING_CONSTRUCTION_TOKEN)
		)
		{
			if( canonicalTraits )
			{
				if( IdentifierIs(t1, FINAL_TOKEN) )
					*canonicalTraits |= asAST_TRAIT_FINAL;
				else if( IdentifierIs(t1, OVERRIDE_TOKEN) )
					*canonicalTraits |= asAST_TRAIT_OVERRIDE;
				else if( IdentifierIs(t1, MIXIN_TOKEN) )
					*canonicalTraits |= asAST_TRAIT_MIXIN;
				else if( IdentifierIs(t1, EXTERNAL_IMPLICIT_THIS_TOKEN) )
					*canonicalTraits |= asAST_TRAIT_EXTERNAL_IMPLICIT_THIS;
				else if( IdentifierIs(t1, GENERATED_FUNCTION_TOKEN) )
					*canonicalTraits |= asAST_TRAIT_GENERATED;
			}
			funcNode->AddChildLast(ParseIdentifier());
		}
		else
		{
			break;
		}
	}
}

#endif

END_AS_NAMESPACE


