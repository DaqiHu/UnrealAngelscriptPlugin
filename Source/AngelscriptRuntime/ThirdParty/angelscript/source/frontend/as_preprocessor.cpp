/*
   Maintained Unreal AngelScript fork — deterministic conditional token routing.
*/

#include "frontend/as_preprocessor.h"

#include "frontend/as_identifier_table.h"

#include "Containers/Set.h"
#include "Containers/StringConv.h"

BEGIN_AS_NAMESPACE

namespace
	{
		constexpr uint64 FnvOffset = 1469598103934665603ull;
		constexpr uint64 FnvPrime = 1099511628211ull;

		uint64 HashByte(uint64 Hash, uint8 Byte)
		{
			return (Hash ^ Byte) * FnvPrime;
		}

		bool TryGetTokenText(
			const asCSourceManager& SourceManager,
			const asCToken& Token,
			FString& OutText)
		{
			TConstArrayView<uint8> Bytes;
			if (!SourceManager.TryGetBytes(Token.Range, Bytes)) { return false; }
			const FUTF8ToTCHAR Converted(
				reinterpret_cast<const ANSICHAR*>(Bytes.GetData()),
				Bytes.Num());
			OutText = FString(Converted.Length(), Converted.Get());
			return true;
		}

		asEDirectiveKind ClassifyDirective(FStringView Text)
		{
#define AS_DIRECTIVE(Name, Spelling) if (Text.Equals(TEXT(Spelling), ESearchCase::CaseSensitive)) { return asEDirectiveKind::Name; }
#include "frontend/as_directive_kinds.def"
#undef AS_DIRECTIVE
			return asEDirectiveKind::Invalid;
		}

		void Report(
			asCDiagnosticFragment& Fragment,
			asEPreprocessorDiagnosticID DiagnosticID,
			const asCSourceRange& Range,
			FStringView Argument = FStringView())
		{
			asSDiagnosticRecord Record;
			Record.DiagnosticID = static_cast<asUINT>(DiagnosticID);
			Record.Severity = asEDiagnosticSeverity::Error;
			Record.PrimaryRange = Range;
			if (!Argument.IsEmpty())
			{
				Record.Arguments.Add(asSDiagnosticArgument::FromText(Argument));
			}
			Fragment.Report(MoveTemp(Record));
		}

		struct asSConditionalFrame
		{
			bool bParentActive = true;
			bool bAnyTaken = false;
			bool bCurrentActive = false;
			bool bSawElse = false;
			asUINT OuterBranchIndex = asSConditionalBranch::InvalidBranchIndex;
			asUINT CurrentBranchIndex = asSConditionalBranch::InvalidBranchIndex;
			asUINT ConditionalIndex = asSConditionalBranch::InvalidBranchIndex;
			asCSourceRange OpenRange;
		};

		struct asSDirectiveLine
		{
			asEDirectiveKind Kind = asEDirectiveKind::Invalid;
			asCSourceRange DirectiveRange;
			asCSourceRange ConditionRange;
			TArray<const asCToken*> Arguments;
			FString AuthoredName;
			asUINT NextTokenIndex = 0;
		};

		bool TryReadDirectiveLine(
			const asCSourceManager& SourceManager,
			TConstArrayView<asCToken> RawTokens,
			asUINT HashIndex,
			asSDirectiveLine& OutLine)
		{
			if (HashIndex >= static_cast<asUINT>(RawTokens.Num())) { return false; }
			const asCToken& Hash = RawTokens[HashIndex];
			const asCSourceFileID FileID = Hash.Range.Begin.FileID;
			const asUINT FileEnd = RawTokens.Last().Range.Begin.ByteOffset;
			TConstArrayView<uint8> TailBytes;
			if (!SourceManager.TryGetBytes(
				SourceManager.MakeRange(FileID, Hash.Range.Begin.ByteOffset, FileEnd),
				TailBytes))
			{
				return false;
			}
			asUINT LineEnd = FileEnd;
			for (asUINT Relative = 0; Relative < static_cast<asUINT>(TailBytes.Num()); ++Relative)
			{
				if (TailBytes[Relative] == '\r' || TailBytes[Relative] == '\n')
				{
					LineEnd = Hash.Range.Begin.ByteOffset + Relative;
					break;
				}
			}

			asUINT NextIndex = HashIndex + 1;
			TArray<const asCToken*> Significant;
			while (NextIndex < static_cast<asUINT>(RawTokens.Num()))
			{
				const asCToken& Token = RawTokens[NextIndex];
				if (Token.Kind == asETokenKind::EndOfFile
					|| Token.Range.Begin.ByteOffset >= LineEnd)
				{
					break;
				}
				if (!asIsTrivia(Token.Kind)) { Significant.Add(&Token); }
				++NextIndex;
			}

			OutLine.DirectiveRange = SourceManager.MakeRange(
				FileID,
				Hash.Range.Begin.ByteOffset,
				LineEnd);
			OutLine.NextTokenIndex = NextIndex;
			if (Significant.IsEmpty())
			{
				OutLine.ConditionRange = OutLine.DirectiveRange;
				return true;
			}

			if (!TryGetTokenText(SourceManager, *Significant[0], OutLine.AuthoredName))
			{
				return false;
			}
			OutLine.Kind = ClassifyDirective(OutLine.AuthoredName);
			for (int32 Index = 1; Index < Significant.Num(); ++Index)
			{
				OutLine.Arguments.Add(Significant[Index]);
			}
			if (OutLine.Arguments.IsEmpty())
			{
				OutLine.ConditionRange = SourceManager.MakeRange(
					FileID,
					Significant[0]->Range.End.ByteOffset,
					Significant[0]->Range.End.ByteOffset);
			}
			else
			{
				OutLine.ConditionRange = SourceManager.MakeRange(
					FileID,
					OutLine.Arguments[0]->Range.Begin.ByteOffset,
					OutLine.Arguments.Last()->Range.End.ByteOffset);
			}
			return true;
		}

		bool IsConditionDirective(asEDirectiveKind Kind)
		{
			return Kind == asEDirectiveKind::If
				|| Kind == asEDirectiveKind::Ifdef
				|| Kind == asEDirectiveKind::Ifndef
				|| Kind == asEDirectiveKind::Elif;
		}

		// Like Clang's PPExpressions ValueLive, liveness controls evaluation diagnostics,
		// never syntactic consumption. This language deliberately admits only booleans.
		class asCConditionParser
		{
		public:
			asCConditionParser(const asCSourceManager& InSources,
				const asCPreprocessorConfiguration& InConfiguration,
				const asSDirectiveLine& InLine, asCDiagnosticFragment& InDiagnostics)
				: Sources(InSources), Configuration(InConfiguration), Line(InLine), Diagnostics(InDiagnostics) {}

			bool Parse(bool bEvaluate, bool& OutValue)
			{
				OutValue = ParseOr(bEvaluate, 0);
				if (bValid && Index != Line.Arguments.Num()) { Fail(CurrentRange()); }
				return bValid;
			}

		private:
			asCSourceRange CurrentRange() const
			{
				return Index < Line.Arguments.Num() ? Line.Arguments[Index]->Range
					: Sources.MakeRange(Line.ConditionRange.End.FileID,
						Line.ConditionRange.End.ByteOffset, Line.ConditionRange.End.ByteOffset);
			}

			bool Consume(asETokenKind Kind)
			{
				if (Index < Line.Arguments.Num() && Line.Arguments[Index]->Kind == Kind)
				{
					++Index;
					return true;
				}
				return false;
			}

			void Fail(const asCSourceRange& Range)
			{
				if (bValid) { Report(Diagnostics, asEPreprocessorDiagnosticID::MalformedCondition, Range); }
				bValid = false;
			}

			bool ParseOr(bool bEvaluate, asUINT Depth)
			{
				bool Value = ParseAnd(bEvaluate, Depth);
				while (bValid && Consume(asETokenKind::PipePipe))
				{
					const bool Right = ParseAnd(bEvaluate && !Value, Depth);
					Value = Value || Right;
				}
				return Value;
			}

			bool ParseAnd(bool bEvaluate, asUINT Depth)
			{
				bool Value = ParseUnary(bEvaluate, Depth);
				while (bValid && Consume(asETokenKind::AmpAmp))
				{
					const bool Right = ParseUnary(bEvaluate && Value, Depth);
					Value = Value && Right;
				}
				return Value;
			}

			bool ParseUnary(bool bEvaluate, asUINT Depth)
			{
				bool bNegate = false;
				while (Consume(asETokenKind::Bang)) { bNegate = !bNegate; }
				const bool Value = ParsePrimary(bEvaluate, Depth);
				return bNegate ? !Value : Value;
			}

			bool ParsePrimary(bool bEvaluate, asUINT Depth)
			{
				if (Depth >= 128 || Index >= Line.Arguments.Num())
				{
					Fail(CurrentRange());
					return false;
				}
				if (Consume(asETokenKind::LeftParen))
				{
					const bool Value = ParseOr(bEvaluate, Depth + 1);
					if (bValid && !Consume(asETokenKind::RightParen)) { Fail(CurrentRange()); }
					return Value;
				}
				const asCToken& Token = *Line.Arguments[Index++];
				FString Text;
				if (!TryGetTokenText(Sources, Token, Text)) { Fail(Token.Range); return false; }
				if (Token.Kind == asETokenKind::NumericLiteral)
				{
					if (Text == TEXT("0")) { return false; }
					if (Text == TEXT("1")) { return true; }
					Fail(Token.Range);
					return false;
				}
				if (!Token.Identifier) { Fail(Token.Range); return false; }
				if (Text == TEXT("defined"))
				{
					const bool bParenthesized = Consume(asETokenKind::LeftParen);
					if (Index >= Line.Arguments.Num() || !Line.Arguments[Index]->Identifier)
					{
						Fail(CurrentRange());
						return false;
					}
					const asCToken& Name = *Line.Arguments[Index++];
					if (!TryGetTokenText(Sources, Name, Text)) { Fail(Name.Range); return false; }
					if (bParenthesized && !Consume(asETokenKind::RightParen))
					{
						Fail(CurrentRange());
						return false;
					}
					return bEvaluate && Configuration.ContainsFlag(Text);
				}
				bool Value = false;
				if (bEvaluate && !Configuration.TryGetFlag(Text, Value))
				{
					Report(Diagnostics, asEPreprocessorDiagnosticID::UnknownFlag, Token.Range, Text);
					bValid = false;
				}
				return Value;
			}

			const asCSourceManager& Sources;
			const asCPreprocessorConfiguration& Configuration;
			const asSDirectiveLine& Line;
			asCDiagnosticFragment& Diagnostics;
			int32 Index = 0;
			bool bValid = true;
		};

		asEConditionEvaluation EvaluateCondition(
			const asCSourceManager& SourceManager,
			const asCPreprocessorConfiguration& Configuration,
			const asSDirectiveLine& Line,
			bool bShouldEvaluate,
			asCDiagnosticFragment& Diagnostics,
			bool& bOutValid)
		{
			if (Line.Kind == asEDirectiveKind::If || Line.Kind == asEDirectiveKind::Elif)
			{
				asCConditionParser Parser(SourceManager, Configuration, Line, Diagnostics);
				bool bValue = false;
				if (!Parser.Parse(bShouldEvaluate, bValue))
				{
					bOutValid = false;
					return asEConditionEvaluation::Invalid;
				}
				if (!bShouldEvaluate) { return asEConditionEvaluation::NotEvaluated; }
				return bValue ? asEConditionEvaluation::True : asEConditionEvaluation::False;
			}
			const asCToken* FlagToken = Line.Arguments.Num() == 1 ? Line.Arguments[0] : nullptr;
			if (FlagToken == nullptr || FlagToken->Identifier == nullptr)
			{
				Report(Diagnostics, asEPreprocessorDiagnosticID::MalformedCondition, Line.ConditionRange);
				bOutValid = false;
				return asEConditionEvaluation::Invalid;
			}

			if (!bShouldEvaluate) { return asEConditionEvaluation::NotEvaluated; }
			FString FlagName;
			if (!TryGetTokenText(SourceManager, *FlagToken, FlagName))
			{
				Report(Diagnostics, asEPreprocessorDiagnosticID::MalformedCondition, FlagToken->Range);
				bOutValid = false;
				return asEConditionEvaluation::Invalid;
			}

			bool bValue = Configuration.ContainsFlag(FlagName);
			if (Line.Kind == asEDirectiveKind::Ifndef) { bValue = !bValue; }
			return bValue ? asEConditionEvaluation::True : asEConditionEvaluation::False;
		}

		bool ParseRestriction(
			const asCSourceManager& SourceManager,
			const asSDirectiveLine& Line,
			asUINT DirectiveRecordIndex,
			asUINT BranchIndex,
			asCDiagnosticFragment& Diagnostics,
			asSRestrictionDirective& OutRestriction)
		{
			FString Operation;
			if (Line.Arguments.IsEmpty()
				|| !TryGetTokenText(SourceManager, *Line.Arguments[0], Operation)
				|| Operation != TEXT("usage"))
			{
				const asCSourceRange& Range = Line.Arguments.IsEmpty()
					? Line.ConditionRange
					: Line.Arguments[0]->Range;
				Report(Diagnostics, asEPreprocessorDiagnosticID::InvalidRestrictionOperation, Range);
				return false;
			}

			FString Policy;
			if (Line.Arguments.Num() < 2
				|| !TryGetTokenText(SourceManager, *Line.Arguments[1], Policy)
				|| (Policy != TEXT("allow") && Policy != TEXT("disallow")))
			{
				const asCSourceRange& Range = Line.Arguments.Num() < 2
					? Line.ConditionRange
					: Line.Arguments[1]->Range;
				Report(Diagnostics, asEPreprocessorDiagnosticID::InvalidRestrictionPolicy, Range);
				return false;
			}

			if (Line.Arguments.Num() < 3)
			{
				Report(Diagnostics, asEPreprocessorDiagnosticID::MissingRestrictionPattern, Line.ConditionRange);
				return false;
			}
			if (Line.Arguments.Num() > 3)
			{
				Report(Diagnostics, asEPreprocessorDiagnosticID::ExtraRestrictionArgument, Line.Arguments[3]->Range);
				return false;
			}

			FString Pattern;
			if (!TryGetTokenText(SourceManager, *Line.Arguments[2], Pattern) || Pattern.IsEmpty())
			{
				Report(Diagnostics, asEPreprocessorDiagnosticID::MissingRestrictionPattern, Line.Arguments[2]->Range);
				return false;
			}
			OutRestriction.Policy = Policy == TEXT("allow")
				? asERestrictionPolicy::Allow
				: asERestrictionPolicy::Disallow;
			OutRestriction.Pattern = MoveTemp(Pattern);
			OutRestriction.DirectiveRange = Line.DirectiveRange;
			OutRestriction.PatternRange = Line.Arguments[2]->Range;
			OutRestriction.DirectiveRecordIndex = DirectiveRecordIndex;
			OutRestriction.BranchIndex = BranchIndex;
			OutRestriction.bActive = true;
			return true;
		}
	}

	asCPreprocessorConfiguration::asCPreprocessorConfiguration(TMap<FString, bool> InFlags)
	{
		Flags.Reserve(InFlags.Num());
		for (TPair<FString, bool>& Pair : InFlags)
		{
			asSPreprocessorFlag& Flag = Flags.AddDefaulted_GetRef();
			Flag.Name = MoveTemp(Pair.Key);
			Flag.bEnabled = Pair.Value;
		}
		Flags.Sort([](const asSPreprocessorFlag& Left, const asSPreprocessorFlag& Right)
		{
			return Left.Name.Compare(Right.Name, ESearchCase::CaseSensitive) < 0;
		});

		uint64 Hash = FnvOffset;
		for (const asSPreprocessorFlag& Flag : Flags)
		{
			const FTCHARToUTF8 Utf8(*Flag.Name);
			for (int32 Index = 0; Index < Utf8.Length(); ++Index)
			{
				Hash = HashByte(Hash, static_cast<uint8>(Utf8.Get()[Index]));
			}
			Hash = HashByte(Hash, 0);
			Hash = HashByte(Hash, Flag.bEnabled ? 1 : 0);
		}
		ConfigurationDigest = Hash;
	}

	bool asCPreprocessorConfiguration::ContainsFlag(FStringView Name) const
	{
		bool bIgnored = false;
		return TryGetFlag(Name, bIgnored);
	}

	bool asCPreprocessorConfiguration::TryGetFlag(FStringView Name, bool& OutEnabled) const
	{
		const FString OwnedName(Name);
		for (const asSPreprocessorFlag& Flag : Flags)
		{
			if (Flag.Name.Equals(OwnedName, ESearchCase::CaseSensitive))
			{
				OutEnabled = Flag.bEnabled;
				return true;
			}
		}
		return false;
	}

	asCPreprocessor::asCPreprocessor(
		const asCSourceManager& InSourceManager,
		const asCPreprocessorConfiguration& InConfiguration,
		asCDiagnosticsEngine& InDiagnostics)
		: SourceManager(InSourceManager)
		, Configuration(InConfiguration)
		, Diagnostics(&InDiagnostics)
	{
	}

	asSDirectiveProcessResult asCPreprocessor::Process(TConstArrayView<asCToken> InputTokens) const
	{
		asSDirectiveProcessResult Result;
		Result.SnapshotID = SourceManager.GetSnapshotID();
		Result.ConfigurationDigest = Configuration.GetDigest();
		Result.SourceSnapshot = SourceManager.GetSnapshot();
		if (!SourceManager.IsReady() || InputTokens.IsEmpty() || Diagnostics == nullptr)
		{
			return Result;
		}
		Result.IdentifierOwner = MakeShared<asCIdentifierTable, ESPMode::ThreadSafe>();
		Result.RawTokens.Append(InputTokens);
		for (asCToken& Token : Result.RawTokens)
		{
			if (Token.Identifier)
			{
				Token.Identifier = Result.IdentifierOwner->Intern(Token.Identifier->GetSpelling());
			}
		}
		const TConstArrayView<asCToken> RawTokens = MakeArrayView(Result.RawTokens);

		asCDiagnosticFragment DiagnosticFragment = Diagnostics->CreateFragment(TEXT("preprocessor"));
		TArray<asSConditionalFrame> Stack;
		bool bValid = true;
		bool bStructureValid = true;
		const asCSourceFileID FileID = RawTokens[0].Range.Begin.FileID;
		const asUINT FileEnd = RawTokens.Last().Range.Begin.ByteOffset;
		asUINT CodeChunkBegin = RawTokens[0].Range.Begin.ByteOffset;
		auto AddCodeChunk = [&Result, &Stack, this, FileID](asUINT Begin, asUINT End)
		{
			if (Begin >= End) { return; }
			asSDirectiveCodeChunk& Chunk = Result.DirectiveTree.CodeChunks.AddDefaulted_GetRef();
			Chunk.Range = SourceManager.MakeRange(FileID, Begin, End);
			Chunk.BranchIndex = Stack.IsEmpty()
				? asSConditionalBranch::InvalidBranchIndex
				: Stack.Last().CurrentBranchIndex;
			Chunk.bActive = Stack.IsEmpty() || Stack.Last().bCurrentActive;
		};
		auto CloseBranch = [&Result, this, FileID](asUINT BranchIndex, asUINT End)
		{
			if (!Result.DirectiveTree.Branches.IsValidIndex(static_cast<int32>(BranchIndex))) { return; }
			asSConditionalBranch& Branch = Result.DirectiveTree.Branches[BranchIndex];
			Branch.BodyRange = SourceManager.MakeRange(
				FileID,
				Branch.DirectiveRange.End.ByteOffset,
				End);
		};
		asUINT TokenIndex = 0;
		while (TokenIndex < static_cast<asUINT>(RawTokens.Num()))
		{
			const asCToken& Token = RawTokens[TokenIndex];
			if (Token.Kind == asETokenKind::EndOfFile) { break; }
			const bool bDirectiveStart = Token.Kind == asETokenKind::Hash
				&& EnumHasAnyFlags(Token.Flags, asETokenFlags::StartOfLine);
			if (!bDirectiveStart)
			{
				const bool bActive = Stack.IsEmpty() || Stack.Last().bCurrentActive;
				if (bActive && !asIsTrivia(Token.Kind)) { Result.ActiveTokens.Add(Token); }
				++TokenIndex;
				continue;
			}

			asSDirectiveLine Line;
			if (!TryReadDirectiveLine(SourceManager, RawTokens, TokenIndex, Line))
			{
				Report(DiagnosticFragment, asEPreprocessorDiagnosticID::UnsupportedDirective, Token.Range);
				bValid = false;
				++TokenIndex;
				continue;
			}
			AddCodeChunk(CodeChunkBegin, Line.DirectiveRange.Begin.ByteOffset);
			CodeChunkBegin = Line.DirectiveRange.End.ByteOffset;
			TokenIndex = Line.NextTokenIndex;
			if (Line.Kind == asEDirectiveKind::Invalid)
			{
				asSDirectiveRecord& Directive = Result.DirectiveTree.DirectiveRecords.AddDefaulted_GetRef();
				Directive.Kind = Line.Kind;
				Directive.DirectiveRange = Line.DirectiveRange;
				Directive.ArgumentRange = Line.ConditionRange;
				Report(
					DiagnosticFragment,
					asEPreprocessorDiagnosticID::UnsupportedDirective,
					Line.DirectiveRange,
					Line.AuthoredName);
				bValid = false;
				continue;
			}

			if (Line.Kind == asEDirectiveKind::If
				|| Line.Kind == asEDirectiveKind::Ifdef
				|| Line.Kind == asEDirectiveKind::Ifndef)
			{
				asSConditionalFrame Frame;
				Frame.bParentActive = Stack.IsEmpty() || Stack.Last().bCurrentActive;
				Frame.OuterBranchIndex = Stack.IsEmpty()
					? asSConditionalBranch::InvalidBranchIndex
					: Stack.Last().CurrentBranchIndex;
				Frame.ConditionalIndex = static_cast<asUINT>(Result.DirectiveTree.Conditionals.Num());
				Frame.OpenRange = Line.DirectiveRange;
				const asEConditionEvaluation Evaluation = EvaluateCondition(
					SourceManager,
					Configuration,
					Line,
					Frame.bParentActive,
					DiagnosticFragment,
					bValid);
				Frame.bCurrentActive = Frame.bParentActive && Evaluation == asEConditionEvaluation::True;
				Frame.bAnyTaken = Frame.bCurrentActive;

				asSConditionalDirective& Conditional = Result.DirectiveTree.Conditionals.AddDefaulted_GetRef();
				Conditional.ParentConditionalIndex = Stack.IsEmpty()
					? asSConditionalBranch::InvalidBranchIndex
					: Stack.Last().ConditionalIndex;
				Conditional.ParentBranchIndex = Frame.OuterBranchIndex;
				Conditional.OpenDirectiveRange = Line.DirectiveRange;
				Conditional.FullRange = Line.DirectiveRange;

				Frame.CurrentBranchIndex = static_cast<asUINT>(Result.DirectiveTree.Branches.Num());
				asSConditionalBranch& Branch = Result.DirectiveTree.Branches.AddDefaulted_GetRef();
				Branch.IntroducerKind = Line.Kind;
				Branch.DirectiveRange = Line.DirectiveRange;
				Branch.ConditionRange = Line.ConditionRange;
				Branch.BodyRange = SourceManager.MakeRange(
					FileID,
					Line.DirectiveRange.End.ByteOffset,
					Line.DirectiveRange.End.ByteOffset);
				Branch.Evaluation = Evaluation;
				Branch.ConditionalIndex = Frame.ConditionalIndex;
				Branch.ParentBranchIndex = Frame.OuterBranchIndex;
				Branch.Depth = static_cast<asUINT>(Stack.Num());
				Branch.bTaken = Frame.bCurrentActive;
				Conditional.BranchIndices.Add(Frame.CurrentBranchIndex);
				if (Frame.bCurrentActive) { Conditional.TakenBranchIndex = Frame.CurrentBranchIndex; }

				asSDirectiveRecord& Directive = Result.DirectiveTree.DirectiveRecords.AddDefaulted_GetRef();
				Directive.Kind = Line.Kind;
				Directive.DirectiveRange = Line.DirectiveRange;
				Directive.ArgumentRange = Line.ConditionRange;
				Directive.ConditionalIndex = Frame.ConditionalIndex;
				Directive.BranchIndex = Frame.CurrentBranchIndex;
				Stack.Add(Frame);
				continue;
			}

			if (Line.Kind == asEDirectiveKind::Elif)
			{
				if (Stack.IsEmpty())
				{
					asSDirectiveRecord& Directive = Result.DirectiveTree.DirectiveRecords.AddDefaulted_GetRef();
					Directive.Kind = Line.Kind;
					Directive.DirectiveRange = Line.DirectiveRange;
					Directive.ArgumentRange = Line.ConditionRange;
					Report(DiagnosticFragment, asEPreprocessorDiagnosticID::UnexpectedElif, Line.DirectiveRange);
					bValid = false;
					bStructureValid = false;
					continue;
				}
				asSConditionalFrame& Frame = Stack.Last();
				CloseBranch(Frame.CurrentBranchIndex, Line.DirectiveRange.Begin.ByteOffset);
				if (Frame.bSawElse)
				{
					Report(DiagnosticFragment, asEPreprocessorDiagnosticID::ElifAfterElse, Line.DirectiveRange);
					bValid = false;
					bStructureValid = false;
				}
				const bool bShouldEvaluate = Frame.bParentActive && !Frame.bAnyTaken && !Frame.bSawElse;
				const asEConditionEvaluation Evaluation = EvaluateCondition(
					SourceManager,
					Configuration,
					Line,
					bShouldEvaluate,
					DiagnosticFragment,
					bValid);
				Frame.bCurrentActive = bShouldEvaluate && Evaluation == asEConditionEvaluation::True;
				Frame.bAnyTaken |= Frame.bCurrentActive;

				asSConditionalBranch& Branch = Result.DirectiveTree.Branches.AddDefaulted_GetRef();
				Branch.IntroducerKind = Line.Kind;
				Branch.DirectiveRange = Line.DirectiveRange;
				Branch.ConditionRange = Line.ConditionRange;
				Branch.BodyRange = SourceManager.MakeRange(
					FileID,
					Line.DirectiveRange.End.ByteOffset,
					Line.DirectiveRange.End.ByteOffset);
				Branch.Evaluation = Evaluation;
				Branch.ConditionalIndex = Frame.ConditionalIndex;
				Branch.ParentBranchIndex = Frame.OuterBranchIndex;
				Branch.Depth = static_cast<asUINT>(Stack.Num() - 1);
				Branch.bTaken = Frame.bCurrentActive;
				Frame.CurrentBranchIndex = static_cast<asUINT>(Result.DirectiveTree.Branches.Num() - 1);
				asSConditionalDirective& Conditional = Result.DirectiveTree.Conditionals[Frame.ConditionalIndex];
				Conditional.BranchIndices.Add(Frame.CurrentBranchIndex);
				if (Frame.bCurrentActive) { Conditional.TakenBranchIndex = Frame.CurrentBranchIndex; }

				asSDirectiveRecord& Directive = Result.DirectiveTree.DirectiveRecords.AddDefaulted_GetRef();
				Directive.Kind = Line.Kind;
				Directive.DirectiveRange = Line.DirectiveRange;
				Directive.ArgumentRange = Line.ConditionRange;
				Directive.ConditionalIndex = Frame.ConditionalIndex;
				Directive.BranchIndex = Frame.CurrentBranchIndex;
				continue;
			}

			if (Line.Kind == asEDirectiveKind::Else)
			{
				if (!Line.Arguments.IsEmpty())
				{
					Report(DiagnosticFragment, asEPreprocessorDiagnosticID::MalformedCondition, Line.ConditionRange);
					bValid = false;
				}
				if (Stack.IsEmpty())
				{
					asSDirectiveRecord& Directive = Result.DirectiveTree.DirectiveRecords.AddDefaulted_GetRef();
					Directive.Kind = Line.Kind;
					Directive.DirectiveRange = Line.DirectiveRange;
					Directive.ArgumentRange = Line.ConditionRange;
					Report(DiagnosticFragment, asEPreprocessorDiagnosticID::UnexpectedElse, Line.DirectiveRange);
					bValid = false;
					bStructureValid = false;
					continue;
				}
				asSConditionalFrame& Frame = Stack.Last();
				CloseBranch(Frame.CurrentBranchIndex, Line.DirectiveRange.Begin.ByteOffset);
				if (Frame.bSawElse)
				{
					Report(DiagnosticFragment, asEPreprocessorDiagnosticID::DuplicateElse, Line.DirectiveRange);
					bValid = false;
					bStructureValid = false;
				}
				const bool bTake = Frame.bParentActive && !Frame.bAnyTaken && !Frame.bSawElse;
				Frame.bSawElse = true;
				Frame.bCurrentActive = bTake;
				Frame.bAnyTaken |= bTake;

				asSConditionalBranch& Branch = Result.DirectiveTree.Branches.AddDefaulted_GetRef();
				Branch.IntroducerKind = Line.Kind;
				Branch.DirectiveRange = Line.DirectiveRange;
				Branch.ConditionRange = Line.ConditionRange;
				Branch.BodyRange = SourceManager.MakeRange(
					FileID,
					Line.DirectiveRange.End.ByteOffset,
					Line.DirectiveRange.End.ByteOffset);
				Branch.Evaluation = Frame.bParentActive
					? (bTake ? asEConditionEvaluation::True : asEConditionEvaluation::False)
					: asEConditionEvaluation::NotEvaluated;
				Branch.ConditionalIndex = Frame.ConditionalIndex;
				Branch.ParentBranchIndex = Frame.OuterBranchIndex;
				Branch.Depth = static_cast<asUINT>(Stack.Num() - 1);
				Branch.bTaken = bTake;
				Frame.CurrentBranchIndex = static_cast<asUINT>(Result.DirectiveTree.Branches.Num() - 1);
				asSConditionalDirective& Conditional = Result.DirectiveTree.Conditionals[Frame.ConditionalIndex];
				Conditional.BranchIndices.Add(Frame.CurrentBranchIndex);
				if (bTake) { Conditional.TakenBranchIndex = Frame.CurrentBranchIndex; }

				asSDirectiveRecord& Directive = Result.DirectiveTree.DirectiveRecords.AddDefaulted_GetRef();
				Directive.Kind = Line.Kind;
				Directive.DirectiveRange = Line.DirectiveRange;
				Directive.ArgumentRange = Line.ConditionRange;
				Directive.ConditionalIndex = Frame.ConditionalIndex;
				Directive.BranchIndex = Frame.CurrentBranchIndex;
				continue;
			}

			if (Line.Kind == asEDirectiveKind::Endif)
			{
				if (!Line.Arguments.IsEmpty())
				{
					Report(DiagnosticFragment, asEPreprocessorDiagnosticID::MalformedCondition, Line.ConditionRange);
					bValid = false;
				}
				if (Stack.IsEmpty())
				{
					asSDirectiveRecord& Directive = Result.DirectiveTree.DirectiveRecords.AddDefaulted_GetRef();
					Directive.Kind = Line.Kind;
					Directive.DirectiveRange = Line.DirectiveRange;
					Directive.ArgumentRange = Line.ConditionRange;
					Directive.bTerminator = true;
					Report(DiagnosticFragment, asEPreprocessorDiagnosticID::UnexpectedEndif, Line.DirectiveRange);
					bValid = false;
					bStructureValid = false;
				}
				else
				{
					const asSConditionalFrame& Frame = Stack.Last();
					CloseBranch(Frame.CurrentBranchIndex, Line.DirectiveRange.Begin.ByteOffset);
					asSConditionalDirective& Conditional = Result.DirectiveTree.Conditionals[Frame.ConditionalIndex];
					Conditional.EndDirectiveRange = Line.DirectiveRange;
					Conditional.FullRange = SourceManager.MakeRange(
						FileID,
						Conditional.OpenDirectiveRange.Begin.ByteOffset,
						Line.DirectiveRange.End.ByteOffset);
					Conditional.bTerminated = true;

					asSDirectiveRecord& Directive = Result.DirectiveTree.DirectiveRecords.AddDefaulted_GetRef();
					Directive.Kind = Line.Kind;
					Directive.DirectiveRange = Line.DirectiveRange;
					Directive.ArgumentRange = Line.ConditionRange;
					Directive.ConditionalIndex = Frame.ConditionalIndex;
					Directive.BranchIndex = Frame.CurrentBranchIndex;
					Directive.bTerminator = true;
					Stack.Pop(EAllowShrinking::No);
				}
				continue;
			}

			if (Line.Kind == asEDirectiveKind::Restrict)
			{
				const asUINT BranchIndex = Stack.IsEmpty()
					? asSConditionalBranch::InvalidBranchIndex
					: Stack.Last().CurrentBranchIndex;
				const asUINT DirectiveRecordIndex =
					static_cast<asUINT>(Result.DirectiveTree.DirectiveRecords.Num());
				asSDirectiveRecord& Directive = Result.DirectiveTree.DirectiveRecords.AddDefaulted_GetRef();
				Directive.Kind = Line.Kind;
				Directive.DirectiveRange = Line.DirectiveRange;
				Directive.ArgumentRange = Line.ConditionRange;
				Directive.ConditionalIndex = Stack.IsEmpty()
					? asSConditionalBranch::InvalidBranchIndex
					: Stack.Last().ConditionalIndex;
				Directive.BranchIndex = BranchIndex;

				const bool bActive = Stack.IsEmpty() || Stack.Last().bCurrentActive;
				if (bActive)
				{
					asSRestrictionDirective Restriction;
					if (ParseRestriction(
						SourceManager,
						Line,
						DirectiveRecordIndex,
						BranchIndex,
						DiagnosticFragment,
						Restriction))
					{
						Result.DirectiveTree.Restrictions.Add(MoveTemp(Restriction));
					}
					else
					{
						bValid = false;
					}
				}
				continue;
			}

			if (Line.Kind == asEDirectiveKind::Include)
			{
				asSDirectiveRecord& Directive = Result.DirectiveTree.DirectiveRecords.AddDefaulted_GetRef();
				Directive.Kind = Line.Kind;
				Directive.DirectiveRange = Line.DirectiveRange;
				Directive.ArgumentRange = Line.ConditionRange;
				Directive.ConditionalIndex = Stack.IsEmpty()
					? asSConditionalBranch::InvalidBranchIndex
					: Stack.Last().ConditionalIndex;
				Directive.BranchIndex = Stack.IsEmpty()
					? asSConditionalBranch::InvalidBranchIndex
					: Stack.Last().CurrentBranchIndex;
				if (Stack.IsEmpty() || Stack.Last().bCurrentActive)
				{
					Report(DiagnosticFragment, asEPreprocessorDiagnosticID::UnsupportedInclude, Line.DirectiveRange);
					bValid = false;
				}
				continue;
			}

			asSDirectiveRecord& Directive = Result.DirectiveTree.DirectiveRecords.AddDefaulted_GetRef();
			Directive.Kind = Line.Kind;
			Directive.DirectiveRange = Line.DirectiveRange;
			Directive.ArgumentRange = Line.ConditionRange;
			Directive.ConditionalIndex = Stack.IsEmpty()
				? asSConditionalBranch::InvalidBranchIndex
				: Stack.Last().ConditionalIndex;
			Directive.BranchIndex = Stack.IsEmpty()
				? asSConditionalBranch::InvalidBranchIndex
				: Stack.Last().CurrentBranchIndex;
			Report(
				DiagnosticFragment,
				asEPreprocessorDiagnosticID::UnsupportedDirective,
				Line.DirectiveRange,
				Line.AuthoredName);
			bValid = false;
		}

		AddCodeChunk(CodeChunkBegin, FileEnd);
		for (const asSConditionalFrame& Frame : Stack)
		{
			CloseBranch(Frame.CurrentBranchIndex, FileEnd);
			asSConditionalDirective& Conditional = Result.DirectiveTree.Conditionals[Frame.ConditionalIndex];
			Conditional.FullRange = SourceManager.MakeRange(
				FileID,
				Conditional.OpenDirectiveRange.Begin.ByteOffset,
				FileEnd);
			Report(DiagnosticFragment, asEPreprocessorDiagnosticID::MissingEndif, Frame.OpenRange);
			bValid = false;
			bStructureValid = false;
		}

		for (asUINT BranchIndex = 0;
			BranchIndex < static_cast<asUINT>(Result.DirectiveTree.Branches.Num());
			++BranchIndex)
		{
			const asSConditionalBranch& Branch = Result.DirectiveTree.Branches[BranchIndex];
			const bool bParentTaken = Branch.ParentBranchIndex == asSConditionalBranch::InvalidBranchIndex
				|| (Result.DirectiveTree.Branches.IsValidIndex(static_cast<int32>(Branch.ParentBranchIndex))
					&& Result.DirectiveTree.Branches[Branch.ParentBranchIndex].bTaken);
			if (!Branch.bTaken && bParentTaken && Branch.BodyRange.Length() > 0)
			{
				asSSkippedSourceRange& Skipped = Result.DirectiveTree.SkippedRanges.AddDefaulted_GetRef();
				Skipped.Range = Branch.BodyRange;
				Skipped.BranchIndex = BranchIndex;
			}
		}
		Result.DirectiveTree.SkippedRanges.Sort([](
			const asSSkippedSourceRange& Left,
			const asSSkippedSourceRange& Right)
		{
			return Left.Range.Begin.ByteOffset < Right.Range.Begin.ByteOffset;
		});
		bValid &= Result.DirectiveTree.Finalize(SourceManager.GetSnapshotID(), bStructureValid);
		bValid &= Result.PreprocessingRecord.Finalize(Result.DirectiveTree);
		Diagnostics->Submit(MoveTemp(DiagnosticFragment));
		Result.bValid = bValid;
		if (!bValid)
		{
			Result.ActiveTokens.Reset();
			return Result;
		}
		if (RawTokens.Last().Kind == asETokenKind::EndOfFile)
		{
			Result.ActiveTokens.Add(RawTokens.Last());
		}
		return Result;
	}

END_AS_NAMESPACE
