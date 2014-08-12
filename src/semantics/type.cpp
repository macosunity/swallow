#include "type.h"

USE_SWIFT_NS


Type::Type(const std::wstring& name, Category category, TypePtr keyType, TypePtr valueType)
:name(name), category(category), keyType(keyType), valueType(valueType)
{

}



TypePtr Type::newDictionaryType(TypePtr keyType, TypePtr valueType)
{
    return TypePtr(new Type(L"", Dictionary, keyType, valueType));
}
TypePtr Type::newType(const std::wstring& name, Category category, const TypeDeclarationPtr& reference)
{
    TypePtr ret = TypePtr(new Type(name, category, nullptr, nullptr));
    ret->reference = reference;
    return ret;
}

TypePtr Type::newTypeReference(const TypePtr& innerType)
{
    TypePtr ret(new Type(L"", MetaType, nullptr, nullptr));
    ret->innerType = innerType;
    return ret;
}


TypePtr Type::newArrayType(TypePtr elementType)
{
    return TypePtr(new Type(L"", Array, nullptr, elementType));
}

TypePtr Type::newTuple(const std::vector<TypePtr>& types)
{
    Type* ret = new Type(L"", Tuple, nullptr, nullptr);
    ret->elementTypes = types;
    return TypePtr(ret);
}

TypePtr Type::newFunction(const std::vector<Parameter>& parameters, const TypePtr& returnType, bool hasVariadicParameters)
{
    Type* ret = new Type(L"", Function, nullptr, nullptr);
    ret->parameters = parameters;
    ret->returnType = returnType;
    ret->variadicParameters = hasVariadicParameters;
    return TypePtr(ret);
}

Type::Category Type::getCategory()const
{
    return category;
}



bool Type::isValueType()const
{
    switch(category)
    {
        case Aggregate:
        case Tuple:
        case Struct:
            //TODO: String is not value type
            return true;
        default:
            return false;
    }
}


const std::wstring& Type::getName()const
{
    return name;
}

const std::wstring& Type::getFullName() const
{
    return fullName;
}
TypePtr Type::getParentType()
{
    return parentType;
}

const std::wstring& Type::getModuleName() const
{
    return moduleName;
}
TypePtr Type::getElementType()
{
    return getElementType(0);
}
TypePtr Type::getElementType(int index)
{
    if(index >= elementTypes.size() || index < 0)
        return nullptr;
    return elementTypes[index];
}
int Type::numElementTypes()const
{
    return elementTypes.size();
}
TypePtr Type::getKeyType()
{
    return keyType;
}

TypePtr Type::getValueType()
{
    return valueType;
}
TypeDeclarationPtr Type::getReference()const
{
    if(reference.expired())
        return nullptr;
    return reference.lock();
}

TypePtr Type::getInnerType()
{
    return innerType;
}


TypePtr Type::getReturnType()
{
    return returnType;
}


const std::vector<Type::Parameter>& Type::getParameters()
{
    return parameters;
}

bool Type::hasVariadicParameters()const
{
    return variadicParameters;
}



FunctionOverloadedSymbolPtr Type::getInitializer()
{
    return initializer;
}
Type::SymbolMap& Type::getSymbols()
{
    return symbols;
}

bool Type::operator ==(const Type& rhs)const
{
    if(category != rhs.category)
        return false;
    if(moduleName != rhs.moduleName)
        return false;

    switch(category)
    {
        case Aggregate:
        case Class:
        case Struct:
        case Protocol:
        case Extension:
        case Enum:
            //check name
            if (fullName != rhs.fullName)
                return false;
            if(name != rhs.name)
                return false;
            if(getReference() != rhs.getReference())
                return false;
            //TODO: check generic parameters
            break;
        case Array:
        case Tuple:
        {
            if (!elementTypes.empty() && !rhs.elementTypes.empty())
                return false;
            auto iter = elementTypes.begin(), iter2 = rhs.elementTypes.begin();
            for (; iter != elementTypes.end(); iter++, iter2++)
            {
                if (*iter != *iter2)
                    return false;
            }
            break;
        }
        case Dictionary:
            if(*keyType != *rhs.keyType)
                return false;
            if(*valueType != *rhs.valueType)
                return false;
            break;
        case MetaType:
            if(*innerType != *rhs.innerType)
                return false;
            break;
        case Function:
        case Closure:
        {
            if(*returnType != *rhs.returnType)
                return false;
            if(parameters.size() != rhs.parameters.size())
                return false;
            if(variadicParameters != rhs.variadicParameters)
                return false;
            auto iter = parameters.begin(), iter2 = rhs.parameters.begin();
            for(; iter != parameters.end(); iter++, iter2++)
            {
                if(iter->name != iter2->name)
                    return false;
                if(iter->inout != iter2->inout)
                    return false;
                if(*iter->type != *iter2->type)
                    return false;
            }
            break;
        }
    }

    return true;
}

bool Type::operator !=(const Type& rhs)const
{
    return !this->operator==(rhs);
}