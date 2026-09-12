#include "frontend/Compile/as_compilation_session.h"
#include "frontend/AST/as_ast_cast.h"
#include "Containers/Set.h"

BEGIN_AS_NAMESPACE
bool asCCompilationSession::ValidateRecordDeclarations()
	{
		bool Valid = true;
		auto Error = [&](asCDecl& Decl, const TCHAR* Message)
		{
			Valid = false; Decl.MarkInvalid();
			const auto* Named = dyn_cast<asCNamedDecl>(&Decl);
			const FString Coordinate = Named ? Named->GetSemanticKey() : TEXT("record");
			StableDiagnostics.Add(Coordinate + TEXT(":") + Message);
			auto Fragment = Diagnostics.CreateFragment(Coordinate);
			asSDiagnosticRecord Record; Record.DiagnosticID = 4103; Record.Severity = asEDiagnosticSeverity::Error;
			Record.PrimaryRange = Decl.GetSourceRange(); Record.Arguments.Add(asSDiagnosticArgument::FromText(Message));
			Fragment.Report(MoveTemp(Record)); Diagnostics.Submit(MoveTemp(Fragment));
		};
		auto BaseRecord = [&](const asCType* Type) -> asCRecordDecl*
		{
			const auto* Nominal = dyn_cast<asCNominalType>(Type);
			return Nominal ? RecordDeclarations.FindRef(Nominal->GetDeclarationIdentity()) : nullptr;
		};
		TMap<asCRecordDecl*, uint8> Colors;
		TFunction<bool(asCRecordDecl*, int)> Validate = [&](asCRecordDecl* Record, int Depth)
		{
			if (Colors.FindRef(Record) == 2) return !Record->IsInvalid();
			if (Colors.FindRef(Record) == 1 || Depth > 256) { Error(*Record, TEXT("cyclic-or-overdeep-inheritance")); return false; }
			Colors.Add(Record, 1);
			int ObjectBases = 0; TSet<asSStableKey> BaseKeys;
			for (const asCType* Base : Record->GetResolvedBases())
			{
				const auto* Nominal = dyn_cast<asCNominalType>(Base); asSNominalTypeDescriptor Identity;
				if (!Nominal || Context.GetTypeContext()->GetIdentityRegistry().ResolveNominalType(Nominal->GetDeclarationIdentity(), Identity) != asETypeIdentityStatus::Succeeded ||
					(Identity.Kind != asETypeDeclKind::Class && Identity.Kind != asETypeDeclKind::Struct && Identity.Kind != asETypeDeclKind::Interface))
				{ Error(*Record, TEXT("invalid-base-type")); continue; }
				if (BaseKeys.Contains(Nominal->GetDeclarationIdentity())) Error(*Record, TEXT("duplicate-base-type"));
				BaseKeys.Add(Nominal->GetDeclarationIdentity());
				if (Identity.Kind != asETypeDeclKind::Interface && ++ObjectBases > 1) Error(*Record, TEXT("multiple-object-bases"));
				if (Record->GetDeclaredTypeKind() == asETypeDeclKind::Interface && Identity.Kind != asETypeDeclKind::Interface) Error(*Record, TEXT("interface-requires-interface-base"));
				if (asCRecordDecl* Parent = BaseRecord(Base); Parent && !Validate(Parent, Depth + 1)) Record->MarkInvalid();
			}
			Colors.Add(Record, 2); return !Record->IsInvalid();
		};
		for (const auto& Entry : RecordDeclarations) Validate(Entry.Value, 0);
		if (!Valid) return false;
		auto SameParameters = [](const asCFunctionDecl& A, const asCFunctionDecl& B)
		{
			const auto* Left = A.GetResolvedSignature(); const auto* Right = B.GetResolvedSignature();
			if (A.GetName() != B.GetName() || A.IsConstMethod() != B.IsConstMethod() || !Left || !Right || Left->GetParameterTypes().Num() != Right->GetParameterTypes().Num()) return false;
			for (int Index = 0; Index < Left->GetParameterTypes().Num(); ++Index)
				if (Left->GetParameterTypes()[Index] != Right->GetParameterTypes()[Index] || Left->GetParameterPassingModes()[Index] != Right->GetParameterPassingModes()[Index]) return false;
			return true;
		};
		for (const auto& Entry : RecordDeclarations)
		for (asCDecl* Decl : Entry.Value->GetDecls())
		{
			auto* Function = dyn_cast<asCFunctionDecl>(Decl);
			if (!Function || Function->GetFunctionKind() != asEFunctionKind::Ordinary || Function->IsInvalid()) continue;
			bool Overrides = false;
			TSet<const asCRecordDecl*> Visited;
			TFunction<void(const asCRecordDecl*)> InspectBases = [&](const asCRecordDecl* Record)
			{
				if (Visited.Contains(Record)) return;
				Visited.Add(Record);
				for (const asCType* Base : Record->GetResolvedBases())
				{
					const auto* Parent = BaseRecord(Base); if (!Parent) continue;
					for (const asCDecl* Member : Parent->GetDecls())
					if (const auto* Method = dyn_cast<asCFunctionDecl>(Member); Method && Method->GetFunctionKind() == asEFunctionKind::Ordinary && SameParameters(*Function, *Method))
					{
						const bool Conversion = Function->GetName() == TEXT("opConv") || Function->GetName() == TEXT("opImplConv") || Function->GetName() == TEXT("opCast") || Function->GetName() == TEXT("opImplCast");
						if (Method->GetResolvedSignature()->GetReturnType() != Function->GetResolvedSignature()->GetReturnType())
						{
							if (Conversion) continue;
							Error(*Function, TEXT("override-return-type-mismatch"));
						}
						if (Parent->GetDeclaredTypeKind() != asETypeDeclKind::Interface) Overrides = true;
						if ((uint16(Method->GetModifiers()) & uint16(asEFunctionModifiers::Final)) != 0) Error(*Function, TEXT("cannot-override-final-method"));
					}
					InspectBases(Parent);
				}
			};
			InspectBases(Entry.Value);
			if ((uint16(Function->GetModifiers()) & uint16(asEFunctionModifiers::Override)) != 0 && !Overrides) Error(*Function, TEXT("method-does-not-override"));
		}
		return Valid;
	}
END_AS_NAMESPACE
