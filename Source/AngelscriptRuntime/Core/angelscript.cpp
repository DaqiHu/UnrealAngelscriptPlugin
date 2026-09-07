#include "angelscript.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"
#include "as_vm_object.h"
#include "as_objecttype.h"

asITypeInfo* asIScriptObject::GetObjectType() const
{
	if (asCObjectType* VmType = asVmTypeFromPayload(const_cast<asIScriptObject*>(this)))
		return VmType;

	if (asITypeInfo* RawScriptType = UASClass::GetRawScriptObjectType(this))
		return RawScriptType;

	if (asITypeInfo* ScriptStructType = UASStruct::GetScriptTypeFromValue(this))
		return ScriptStructType;

	//return (asITypeInfo*)((UObject*)this)->GetClass()->ScriptTypePtr;
	UASClass* asClass = UASClass::GetFirstASClass((UObject*)this);
	if (asClass)
		return (asITypeInfo*)asClass->ScriptTypePtr;
	else
		return nullptr;
}
