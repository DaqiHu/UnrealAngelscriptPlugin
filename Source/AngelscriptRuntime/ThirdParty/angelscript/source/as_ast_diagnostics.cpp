#include "as_ast_diagnostics.h"

#include "as_ast_public_view.h"

#include <cstring>

BEGIN_AS_NAMESPACE

namespace
{
	struct asSASTDiagnosticSnapshotInfo
	{
		asCString module;
		asCString generation;
		asCString provenance;
		bool retained;
		bool current;
		asUINT declCount;
		asUINT typeCount;
		asUINT stmtCount;
		asUINT exprCount;

		asSASTDiagnosticSnapshotInfo()
			: retained(false)
			, current(false)
			, declCount(0)
			, typeCount(0)
			, stmtCount(0)
			, exprCount(0)
		{
		}
	};

	class asCASTDiagnosticSnapshotLease
	{
	public:
		explicit asCASTDiagnosticSnapshotLease(asIScriptModule* module)
			: snapshot(module ? module->AcquireASTSnapshot(asAST_API_VERSION_1) : 0)
		{
		}

		~asCASTDiagnosticSnapshotLease()
		{
			if( snapshot ) snapshot->Release();
		}

		asCASTSnapshot* Get() const
		{
			return snapshot ? static_cast<asCASTSnapshot*>(snapshot) : 0;
		}

	private:
		asCASTDiagnosticSnapshotLease(const asCASTDiagnosticSnapshotLease&);
		asCASTDiagnosticSnapshotLease& operator=(const asCASTDiagnosticSnapshotLease&);
		asIASTSnapshot* snapshot;
	};

	const char* DiagnosticOperationName(asEASTDiagnosticOperation operation)
	{
		switch( operation )
		{
		case asAST_DIAGNOSTIC_LIST: return "list";
		case asAST_DIAGNOSTIC_DUMP: return "dump";
		case asAST_DIAGNOSTIC_VERIFY: return "verify";
		case asAST_DIAGNOSTIC_QUERY: return "query";
		case asAST_DIAGNOSTIC_DIFF: return "diff";
		default: return "invalid";
		}
	}

	void AppendJsonString(asCString& out, const char* value)
	{
		out += "\"";
		if( value )
		{
			for( const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p; ++p )
			{
				switch( *p )
				{
				case '\\': out += "\\\\"; break;
				case '"': out += "\\\""; break;
				case '\b': out += "\\b"; break;
				case '\f': out += "\\f"; break;
				case '\n': out += "\\n"; break;
				case '\r': out += "\\r"; break;
				case '\t': out += "\\t"; break;
				default:
					if( *p < 0x20 )
					{
						asCString escaped;
						escaped.Format("\\u%04x", static_cast<unsigned int>(*p));
						out += escaped;
					}
					else
					{
						char byte[2] = { static_cast<char>(*p), 0 };
						out += byte;
					}
					break;
				}
			}
		}
		out += "\"";
	}

	asIScriptModule* FindModule(asIScriptEngine* engine, const asCString& name)
	{
		if( engine == 0 || name.GetLength() == 0 ) return 0;
		for( asUINT index = 0; index < engine->GetModuleCount(); ++index )
		{
			asIScriptModule* module = engine->GetModuleByIndex(index);
			const char* moduleName = module ? module->GetName() : 0;
			if( moduleName && std::strcmp(moduleName, name.AddressOf()) == 0 ) return module;
		}
		return 0;
	}

	bool CaptureSnapshotInfo(asCASTSnapshot* snapshot, asSASTDiagnosticSnapshotInfo& out)
	{
		if( snapshot == 0 ) return false;
		const asCASTContext* context = snapshot->GetContext();
		if( context == 0 || !context->IsPublishable() ) return false;
		out.module = snapshot->GetModuleName() ? snapshot->GetModuleName() : "";
		out.generation = snapshot->GetGenerationKey() ? snapshot->GetGenerationKey() : "";
		out.provenance = snapshot->GetProvenance() ? snapshot->GetProvenance() : "unknown";
		out.retained = true;
		out.current = snapshot->IsCurrentGeneration();
		out.declCount = context->GetDeclCount();
		out.typeCount = context->GetTypeCount();
		out.stmtCount = context->GetStmtCount();
		out.exprCount = context->GetExprCount();
		return true;
	}

	void AppendTextEnvelope(asCString& out, asEASTDiagnosticOperation operation)
	{
		out = "ast-diagnostics schema=as-canonical-ast-command-v1 operation=";
		out += DiagnosticOperationName(operation);
		out += " format=text diagnosticOnly=true cacheInput=false readOnly=true mutation=refused lease=acquired-released\n";
	}

	void AppendTextSnapshot(
		asCString& out,
		const asSASTDiagnosticSnapshotInfo& info,
		const char* side = 0)
	{
		out += "snapshot";
		if( side && side[0] )
		{
			out += " side=";
			out += side;
		}
		out += " module=";
		AppendJsonString(out, info.module.AddressOf());
		out += " generation=";
		AppendJsonString(out, info.generation.AddressOf());
		out += " provenance=";
		out += info.provenance;
		asCString facts;
		facts.Format(
			" retained=%s current=%s decls=%u types=%u stmts=%u exprs=%u\n",
			info.retained ? "true" : "false",
			info.current ? "true" : "false",
			info.declCount,
			info.typeCount,
			info.stmtCount,
			info.exprCount);
		out += facts;
	}

	void AppendJsonSnapshot(asCString& out, const asSASTDiagnosticSnapshotInfo& info)
	{
		out += "{\"module\":";
		AppendJsonString(out, info.module.AddressOf());
		out += ",\"generation\":";
		AppendJsonString(out, info.generation.AddressOf());
		out += ",\"provenance\":";
		AppendJsonString(out, info.provenance.AddressOf());
		asCString facts;
		facts.Format(
			",\"retained\":%s,\"current\":%s,\"decls\":%u,\"types\":%u,\"stmts\":%u,\"exprs\":%u}",
			info.retained ? "true" : "false",
			info.current ? "true" : "false",
			info.declCount,
			info.typeCount,
			info.stmtCount,
			info.exprCount);
		out += facts;
	}

	void BeginJsonEnvelope(asCString& out, asEASTDiagnosticOperation operation)
	{
		out = "{\"schema\":\"as-canonical-ast-command-v1\",\"diagnosticOnly\":true,\"cacheInput\":false,\"readOnly\":true,\"mutation\":\"refused\",\"lease\":\"acquired-released\",\"operation\":";
		AppendJsonString(out, DiagnosticOperationName(operation));
	}

	int RenderError(
		const asSASTDiagnosticRequest& request,
		int status,
		const char* detail,
		asCString& out)
	{
		if( request.format == asAST_DIAGNOSTIC_JSON )
		{
			BeginJsonEnvelope(out, request.operation);
			out += ",\"status\":";
			asCString number;
			number.Format("%d,\"error\":", status);
			out += number;
			AppendJsonString(out, detail);
			out += "}";
		}
		else
		{
			AppendTextEnvelope(out, request.operation);
			asCString number;
			number.Format("status=%d error=", status);
			out += number;
			AppendJsonString(out, detail);
			out += "\n";
		}
		return status;
	}

	int RunList(
		asIScriptEngine* engine,
		const asSASTDiagnosticRequest& request,
		asCString& out)
	{
		asCArray<asSASTDiagnosticSnapshotInfo> infos;
		for( asUINT index = 0; index < engine->GetModuleCount(); ++index )
		{
			asIScriptModule* module = engine->GetModuleByIndex(index);
			if( module == 0 ) continue;
			asSASTDiagnosticSnapshotInfo info;
			info.module = module->GetName() ? module->GetName() : "";
			info.provenance = "unavailable";
			{
				asCASTDiagnosticSnapshotLease lease(module);
				(void)CaptureSnapshotInfo(lease.Get(), info);
			}
			infos.PushLast(info);
		}
		for( asUINT left = 0; left < infos.GetLength(); ++left )
		{
			for( asUINT right = left + 1; right < infos.GetLength(); ++right )
			{
				if( std::strcmp(infos[left].module.AddressOf(), infos[right].module.AddressOf()) > 0 )
				{
					asSASTDiagnosticSnapshotInfo temporary = infos[left];
					infos[left] = infos[right];
					infos[right] = temporary;
				}
			}
		}

		if( request.format == asAST_DIAGNOSTIC_JSON )
		{
			BeginJsonEnvelope(out, request.operation);
			out += ",\"modules\":[";
			for( asUINT index = 0; index < infos.GetLength(); ++index )
			{
				if( index ) out += ",";
				AppendJsonSnapshot(out, infos[index]);
			}
			out += "]}";
		}
		else
		{
			AppendTextEnvelope(out, request.operation);
			asCString count;
			count.Format("moduleCount=%u\n", infos.GetLength());
			out += count;
			for( asUINT index = 0; index < infos.GetLength(); ++index )
				AppendTextSnapshot(out, infos[index]);
		}
		return asSUCCESS;
	}

	int RunSingle(
		asIScriptEngine* engine,
		const asSASTDiagnosticRequest& request,
		asCString& out)
	{
		asIScriptModule* module = FindModule(engine, request.moduleName);
		if( module == 0 ) return RenderError(request, asNO_MODULE, "module-not-found", out);
		asSASTDiagnosticSnapshotInfo info;
		asCString payload;
		int status = asSUCCESS;
		{
			asCASTDiagnosticSnapshotLease lease(module);
			asCASTSnapshot* snapshot = lease.Get();
			if( !CaptureSnapshotInfo(snapshot, info) )
				return RenderError(request, asNOT_SUPPORTED, "retained-snapshot-unavailable", out);
			const asCASTContext* context = snapshot->GetContext();
			asSASTStructuredDumpOptions options;
			options.format = request.format == asAST_DIAGNOSTIC_JSON
				? asAST_STRUCTURED_DUMP_JSON
				: asAST_STRUCTURED_DUMP_TREE;
			options.filter = request.filter;
			options.filter.moduleName = request.moduleName;
			options.includeSourceSnippets = request.includeSourceSnippets;
			options.maxSnippetBytes = request.maxSnippetBytes;
			if( request.operation == asAST_DIAGNOSTIC_DUMP )
			{
				status = asCASTStructuredDump(*context, options, payload);
			}
			else if( request.operation == asAST_DIAGNOSTIC_QUERY )
			{
				status = asCASTQuery(*context, options, payload);
			}
			else if( request.operation == asAST_DIAGNOSTIC_VERIFY )
			{
				asSAstVerifyResult result;
				const int verifyStatus = asCASTVerifyPublication(*context, result);
				if( verifyStatus == asAST_VERIFY_OK )
				{
					payload = request.format == asAST_DIAGNOSTIC_JSON
						? "{\"verify\":\"pass\"}"
						: "verify=pass\n";
				}
				else
				{
					asCASTFormatVerifyResult(result, payload);
					status = asERROR;
				}
			}
			else
			{
				status = asINVALID_ARG;
			}
		}
		if( status != asSUCCESS ) return RenderError(request, status, payload.AddressOf(), out);
		if( request.format == asAST_DIAGNOSTIC_JSON )
		{
			BeginJsonEnvelope(out, request.operation);
			out += ",\"snapshot\":";
			AppendJsonSnapshot(out, info);
			out += ",\"result\":";
			out += payload;
			out += "}";
		}
		else
		{
			AppendTextEnvelope(out, request.operation);
			AppendTextSnapshot(out, info);
			out += payload;
		}
		return asSUCCESS;
	}

	int RunDiff(
		asIScriptEngine* engine,
		const asSASTDiagnosticRequest& request,
		asCString& out)
	{
		asIScriptModule* leftModule = FindModule(engine, request.moduleName);
		asIScriptModule* rightModule = FindModule(engine, request.compareModuleName);
		if( leftModule == 0 || rightModule == 0 )
			return RenderError(request, asNO_MODULE, "diff-module-not-found", out);
		asSASTDiagnosticSnapshotInfo leftInfo;
		asSASTDiagnosticSnapshotInfo rightInfo;
		asCString payload;
		{
			asCASTDiagnosticSnapshotLease leftLease(leftModule);
			asCASTDiagnosticSnapshotLease rightLease(rightModule);
			asCASTSnapshot* left = leftLease.Get();
			asCASTSnapshot* right = rightLease.Get();
			if( !CaptureSnapshotInfo(left, leftInfo) || !CaptureSnapshotInfo(right, rightInfo) )
				return RenderError(request, asNOT_SUPPORTED, "diff-retained-snapshot-unavailable", out);
			asCASTShadowDiff(*left->GetContext(), *right->GetContext(), payload);
		}

		if( request.format == asAST_DIAGNOSTIC_JSON )
		{
			BeginJsonEnvelope(out, request.operation);
			out += ",\"left\":";
			AppendJsonSnapshot(out, leftInfo);
			out += ",\"right\":";
			AppendJsonSnapshot(out, rightInfo);
			out += ",\"equivalent\":";
			out += payload == "match" ? "true" : "false";
			out += ",\"result\":";
			AppendJsonString(out, payload.AddressOf());
			out += "}";
		}
		else
		{
			AppendTextEnvelope(out, request.operation);
			AppendTextSnapshot(out, leftInfo, "left");
			AppendTextSnapshot(out, rightInfo, "right");
			out += payload;
			out += "\n";
		}
		return asSUCCESS;
	}
}

int asCASTRunDiagnostics(
	asIScriptEngine* engine,
	const asSASTDiagnosticRequest& request,
	asCString& out)
{
	out = "";
	if( engine == 0 ) return RenderError(request, asINVALID_ARG, "engine-null", out);
	if( request.operation < asAST_DIAGNOSTIC_LIST || request.operation > asAST_DIAGNOSTIC_DIFF )
		return RenderError(request, asINVALID_ARG, "operation-invalid-read-only-surface", out);
	if( request.format != asAST_DIAGNOSTIC_TEXT && request.format != asAST_DIAGNOSTIC_JSON )
		return RenderError(request, asINVALID_ARG, "format-invalid", out);
	if( request.operation == asAST_DIAGNOSTIC_LIST ) return RunList(engine, request, out);
	if( request.moduleName.GetLength() == 0 )
		return RenderError(request, asINVALID_ARG, "module-required", out);
	if( request.operation == asAST_DIAGNOSTIC_QUERY && !request.filter.HasNodePredicates() )
		return RenderError(request, asINVALID_ARG, "query-filter-required", out);
	if( request.operation == asAST_DIAGNOSTIC_DIFF )
	{
		if( request.compareModuleName.GetLength() == 0 )
			return RenderError(request, asINVALID_ARG, "compare-module-required", out);
		return RunDiff(engine, request, out);
	}
	return RunSingle(engine, request, out);
}

END_AS_NAMESPACE
