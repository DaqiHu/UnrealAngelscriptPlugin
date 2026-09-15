/* Maintained Unreal AngelScript fork — owner and freeze boundary for typed AST nodes. */
#pragma once

#include "frontend/AST/as_expr.h"
#include "frontend/Basic/as_source_manager.h"
#include "frontend/AST/as_type.h"
#include "frontend/AST/as_type_context.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"

BEGIN_AS_NAMESPACE

struct asCASTNodeHandle
	{
		uint64 OwnerID = 0;
		uint32 LocalID = 0;
		bool IsValid() const { return OwnerID != 0 && LocalID != 0; }
	};

	class ANGELSCRIPTRUNTIME_API asCASTContext
	{
	public:
		explicit asCASTContext(asCSourceManager InSources);
		asCASTContext(asCSourceManager InSources,
			TSharedRef<asCTypeContext, ESPMode::ThreadSafe> InTypeContext);
		~asCASTContext();

		asCASTContext(const asCASTContext&) = delete;
		asCASTContext& operator=(const asCASTContext&) = delete;

		template <typename T, typename... Args>
		T* Create(Args&&... Arguments)
		{
			FScopeLock Lock(&MutationMutex);
			return CreateLocked<T>(Forward<Args>(Arguments)...);
		}

		asCBuiltinType* GetBuiltinType(asEPrimitiveType Primitive);
		asCNominalType* GetNominalType(
			const asSStableKey& DeclarationIdentity,
			const asSStableKey& TypeUseIdentity);
		asCGenericType* GetGenericType(
			asCType* TemplateType,
			TArray<asCQualType> Arguments,
			const asSStableKey& TypeUseIdentity);
		asCFunctionType* GetFunctionType(
			asCQualType ReturnType,
			TArray<asCQualType> Parameters,
			const asSStableKey& TypeUseIdentity,
			TArray<asEParameterPassingMode> PassingModes = {});
		bool Owns(const asCASTNode* Node) const { return Node && Node->GetOwnerContext() == this; }
		asCASTNodeHandle GetHandle(const asCASTNode* Node) const;
		asCASTNode* Resolve(asCASTNodeHandle Handle) const;
		bool VerifyAndSeal(const asCASTNode* Root, asSASTVerificationResult& OutResult);
		bool FreezeForTooling();
		bool IsSealed() const;
		bool IsFrozenForTooling() const;
		int32 GetNodeCount() const;
		// The view is stable after VerifyAndSeal, or while the caller otherwise
		// provides the same worker-join barrier required before sealing. It is not
		// a promise of concurrent iteration during allocation.
		TConstArrayView<asCASTNode*> GetNodes() const { return MakeArrayView(Nodes); }
		uint64 GetContextID() const { return ContextID; }
		const asCSourceManager& GetSources() const { return Sources; }
		const TSharedRef<asCTypeContext, ESPMode::ThreadSafe>& GetTypeContext() const { return TypeContext; }

	private:
		friend class asCASTVerifier;
		friend class asCASTVerifierAccess;

		template <typename T, typename... Args>
		T* CreateLocked(Args&&... Arguments)
		{
			if (bSealed || bFrozenForTooling) return nullptr;
			T* Result = new T(Forward<Args>(Arguments)...);
			const uint32 LocalID = static_cast<uint32>(Nodes.Num() + 1);
			Result->BindOwner(this, LocalID);
			if (!ValidateTypeIdentity(*Result)) Result->MarkInvalid();
			Nodes.Add(Result);
			asSAllocation Allocation;
			Allocation.Address = Result;
			Allocation.Destroy = [](void* Address) { delete static_cast<T*>(Address); };
			Allocations.Add(Allocation);
			return Result;
		}
		bool ValidateTypeIdentity(const asCASTNode& Node) const;

		struct asSAllocation
		{
			void* Address = nullptr;
			void (*Destroy)(void*) = nullptr;
		};

		uint64 ContextID = 0;
		mutable FCriticalSection MutationMutex;
		bool bSealed = false;
		bool bFrozenForTooling = false;
		asCSourceManager Sources;
		TSharedRef<asCTypeContext, ESPMode::ThreadSafe> TypeContext;
		TArray<asCASTNode*> Nodes;
		TArray<asSAllocation> Allocations;
		TMap<uint8, asCBuiltinType*> BuiltinTypes;
		TMap<asSStableKey, asCType*> CanonicalTypes;
	};

END_AS_NAMESPACE
