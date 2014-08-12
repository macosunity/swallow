#include <semantics/symbol-registry.h>
#include <common/compiler_results.h>
#include <swift_errors.h>
#include "type-inference-action.h"
#include "ast/ast.h"
#include "scoped-nodes.h"
#include "ast/type-node.h"
#include "type.h"
#include "semantic-types.h"
#include "function-symbol.h"
#include "function-overloaded-symbol.h"
#include <cassert>
USE_SWIFT_NS


TypeInferenceAction::TypeInferenceAction(SymbolRegistry* symbolRegistry, CompilerResults* compilerResults)
    :SemanticNodeVisitor(symbolRegistry, compilerResults)
{
    t_int = symbolRegistry->lookupType(L"Int");
    t_uint = symbolRegistry->lookupType(L"UInt");
    t_int8 = symbolRegistry->lookupType(L"Int8");
    t_uint8 = symbolRegistry->lookupType(L"UInt8");
    t_int16 = symbolRegistry->lookupType(L"Int16");
    t_uint16 = symbolRegistry->lookupType(L"UInt16");
    t_int32 = symbolRegistry->lookupType(L"Int32");
    t_uint32 = symbolRegistry->lookupType(L"UInt32");
    t_int64 = symbolRegistry->lookupType(L"Int64");
    t_uint64 = symbolRegistry->lookupType(L"UInt64");


    t_bool = symbolRegistry->lookupType(L"Bool");
    t_double = symbolRegistry->lookupType(L"Double");
    t_float = symbolRegistry->lookupType(L"Float");
    t_string = symbolRegistry->lookupType(L"String");


    t_ints.push_back(t_int);
    t_ints.push_back(t_uint);
    t_ints.push_back(t_int8);
    t_ints.push_back(t_uint8);
    t_ints.push_back(t_int16);
    t_ints.push_back(t_uint16);
    t_ints.push_back(t_int32);
    t_ints.push_back(t_uint32);
    t_ints.push_back(t_int64);
    t_ints.push_back(t_uint64);

    for(const TypePtr& t : t_ints)
    {
        t_numbers.push_back(t);
    }
    t_numbers.push_back(t_double);
    t_numbers.push_back(t_float);

}

void TypeInferenceAction::checkTupleDefinition(const TuplePtr& tuple, const ExpressionPtr& initializer)
{
    //this is a tuple definition, the corresponding declared type must be a tuple type
    TypeNodePtr declaredType = tuple->getDeclaredType();
    TypePtr type = lookupType(declaredType);
    if(!type)
    {
        std::wstringstream out;
        declaredType->serialize(out);
        error(tuple, Errors::E_USE_OF_UNDECLARED_TYPE, out.str());
        return;
    }
    if(!(type->getCategory() == Type::Tuple))
    {
        //tuple definition must have a tuple type definition
        std::wstringstream out;
        declaredType->serialize(out);
        error(tuple, Errors::E_TUPLE_PATTERN_MUST_MATCH_TUPLE_TYPE, out.str());
        return;
    }
    if(tuple->numElements() != type->numElementTypes())
    {
        //tuple pattern has the wrong length for tuple type '%'
        std::wstringstream out;
        declaredType->serialize(out);
        error(tuple, Errors::E_TUPLE_PATTERN_MUST_MATCH_TUPLE_TYPE, out.str());
        return;
    }
    //check if initializer has the same type with the declared type
    if(initializer)
    {
        TypePtr valueType = evaluateType(initializer);
        if(valueType && *valueType != *type)
        {
            //tuple pattern has the wrong length for tuple type '%'
            std::wstringstream out;
            declaredType->serialize(out);
            error(initializer, Errors::E_TUPLE_TYPES_HAVE_A_DIFFERENT_NUMBER_OF_ELEMENT, out.str());
            return;
        }
    }


    for(const PatternPtr& p : *tuple)
    {
        NodeType::T nodeType = p->getNodeType();
        if(nodeType != NodeType::Identifier)
        {

        }

    }
}


TypePtr TypeInferenceAction::getExpressionType(const ExpressionPtr& expr, const TypePtr& hint, float& score)
{
    if(expr->getType() == nullptr)
        expr->accept(this);
    score = 1;
    if(expr->getNodeType() == NodeType::IntegerLiteral && hint != nullptr)
    {
        IntegerLiteralPtr literal = std::static_pointer_cast<IntegerLiteral>(expr);
        if(hint == t_float || hint == t_double)
        {
            score  = 0.5;
            return hint;
        }
        for(const TypePtr& t : t_ints)
        {
            if(t == hint)
                return t;
        }
    }
    if(expr->getNodeType() == NodeType::FloatLiteral && hint != nullptr)
    {
        FloatLiteralPtr literal = std::static_pointer_cast<FloatLiteral>(expr);
        if(hint == t_float || hint == t_double)
            return hint;
    }
    return expr->getType();
}
/**
 *
 * @param parameter
 * @param argument
 * @param variadic  Variadic parameter must have no name
 * @param score
 * @param supressErrors
 */
bool TypeInferenceAction::checkArgument(const Type::Parameter& parameter, const ParenthesizedExpression::Term& argument, bool variadic, float& score, bool supressErrors)
{

    const std::wstring name = argument.first;
    float s = 1;
    TypePtr argType = getExpressionType(argument.second, parameter.type, s);

    if(variadic)
    {
        //variadic parameter must have no name
        if(!name.empty())
        {
            error(argument.second, Errors::E_EXTRANEOUS_ARGUMENT_LABEL_IN_CALL, name);
            abort();
        }
    }
    else
    {
        if (parameter.name != name)
        {
            if (!supressErrors)
            {
                if (name.empty() && !parameter.name.empty())
                {
                    error(argument.second, Errors::E_MISSING_ARGUMENT_LABEL_IN_CALL, parameter.name);
                }
                else
                {
                    error(argument.second, Errors::E_EXTRANEOUS_ARGUMENT_LABEL_IN_CALL, name);
                }
                abort();
            }
            return false;
        }
    }

    if(*argType != *parameter.type)
    {
        if (!supressErrors)
        {
            error(argument.second, Errors::E_UNMATCHED_PARAMETER);
            abort();
        }
        return false;//parameter is not matched
    }
    score += s;
    return true;
}

float TypeInferenceAction::calculateFitScore(const FunctionSymbolPtr& func, const ParenthesizedExpressionPtr& arguments, bool supressErrors)
{
    float score = 0;
    const std::vector<Type::Parameter>& parameters = func->getType()->getParameters();
    bool variadic = func->getType()->hasVariadicParameters();

    //TODO: check trailing closure


    std::vector<Type::Parameter>::const_iterator paramIter = parameters.begin();
    std::vector<ParenthesizedExpression::Term>::const_iterator argumentIter = arguments->begin();
    std::vector<Type::Parameter>::const_iterator paramEnd = variadic ? parameters.end() - 1 : parameters.end();
    for(;argumentIter != arguments->end() && paramIter != paramEnd; argumentIter++, paramIter++)
    {
        const Type::Parameter& parameter = *paramIter;
        const ParenthesizedExpression::Term& argument = *argumentIter;
        bool ret = checkArgument(parameter, argument, false, score, supressErrors);
        if(!ret)
            return -1;
    }
    //if there's the rest parameters, they must have initializer
    if(paramIter != paramEnd && argumentIter == arguments->end())
    {
        //if there's only one parameter and it's a variadic parameter, ignore this error
        if(!(paramIter + 1 == parameters.end() && variadic))
        {
            //TODO: check initializer
            if (!supressErrors)
            {
                error(arguments, Errors::E_UNMATCHED_PARAMETERS);
                abort();
            }
            return -1;
        }
    }
    //if there's the rest arguments, check for variadic
    if(paramIter == paramEnd && argumentIter != arguments->end())
    {
        if(!variadic)
        {
            if(!supressErrors)
            {
                error(argumentIter->second, Errors::E_EXTRANEOUS_ARGUMENT);
                abort();
            }
            return -1;
        }
        const Type::Parameter& parameter = *paramIter;
        //the first variadic argument must have a label if the parameter got a label
        if(!parameter.name.empty())
        {
            bool ret = checkArgument(parameter, *argumentIter++, false, score, supressErrors);
            if(!ret)
                return -1;
        }

        //check rest argument
        for(;argumentIter != arguments->end(); argumentIter++)
        {
            bool ret = checkArgument(parameter, *argumentIter, true, score, supressErrors);
            if(!ret)
                return -1;
        }
    }
    if(!arguments->numExpressions())
        return 1;
    return score / arguments->numExpressions();
}

void TypeInferenceAction::visitFunctionCall(const IdentifierPtr& name, const FunctionCallPtr& node)
{
    SymbolPtr sym = symbolRegistry->lookupSymbol(name->getIdentifier());
    if(!sym)
    {
        error(name, Errors::E_USE_OF_UNRESOLVED_IDENTIFIER, name->getIdentifier());
        abort();
        return;
    }
    //Prepare the arguments
    for(const ParenthesizedExpression::Term& term : *node->getArguments())
    {
        term.second->accept(this);
    }

    if(FunctionSymbolPtr func = std::dynamic_pointer_cast<FunctionSymbol>(sym))
    {
        //verify argument
        calculateFitScore(func, node->getArguments(), false);
        node->setType(func->getReturnType());
    }
    else if(FunctionOverloadedSymbolPtr funcs = std::dynamic_pointer_cast<FunctionOverloadedSymbol>(sym))
    {
        typedef std::pair<float, FunctionSymbolPtr> ScoredFunction;
        std::vector<ScoredFunction> candidates;
        for(FunctionSymbolPtr func : *funcs)
        {
            float score = calculateFitScore(func, node->getArguments(), true);
            if(score > 0)
                candidates.push_back(std::make_pair(score, func));
        }
        if(candidates.empty())
        {
            error(node, Errors::E_NO_MATCHED_OVERLOAD);
            abort();
        }
        //sort by fit score
        if(candidates.size() > 1)
        {
            sort(candidates.begin(), candidates.end(), [](const ScoredFunction& lhs, const ScoredFunction& rhs ){
                return rhs.first < lhs.first;
            });
            if(candidates[0].first == candidates[1].first)
            {
                error(node, Errors::E_AMBIGUOUS_USE, name->getIdentifier());
                abort();
            }
        }
        FunctionSymbolPtr matched = candidates.front().second;
        node->setType(matched->getReturnType());
    }
    else
    {
        assert(0 && "Unsupported function to call");
    }

}
void TypeInferenceAction::visitFunctionCall(const FunctionCallPtr& node)
{
    if(node->getFunction()->getNodeType() == NodeType::Identifier)
    {
        IdentifierPtr func = std::static_pointer_cast<Identifier>(node->getFunction());
        visitFunctionCall(func, node);
        return;
    }


    SemanticNodeVisitor::visitFunctionCall(node);
    ExpressionPtr func = node->getFunction();

    TypePtr t = func->getType();
    Type::Category category = t->getCategory();

    if(category == Type::Function || category == Type::Closure)
    {
        //TODO: type reference for calling function
        node->setType(t->getReturnType());
    }
    else if(category == Type::MetaType)
    {
        //initiate a new instance of specified type
        node->setType(t->getInnerType());
    }
    else
    {
        //it's not a function, cannot call
        std::wstringstream out;
        func->serialize(out);
        error(func, Errors::E_INVALID_CALL_OF_NON_FUNCTION_TYPE, out.str());
        abort();
    }


}
void TypeInferenceAction::visitString(const StringLiteralPtr& node)
{
    node->setType(t_string);
}
void TypeInferenceAction::visitInteger(const IntegerLiteralPtr& node)
{
    node->setType(t_int);
}
void TypeInferenceAction::visitFloat(const FloatLiteralPtr& node)
{
    node->setType(t_double);
}
void TypeInferenceAction::visitOperator(const OperatorDefPtr& node)
{
    //register operator
    if(node->getType() == OperatorType::InfixBinary)
    {
        //check precedence range
        if(node->getPrecedence() < 0 || node->getPrecedence() > 255)
        {
            error(node, Errors::E_OPERATOR_PRECEDENCE_OUT_OF_RANGE);
            abort();
        }
    }

    bool r = symbolRegistry->registerOperator(node->getName(), node->getType(), node->getAssociativity(), node->getPrecedence());
    if(!r)
    {
        error(node, Errors::E_OPERATOR_REDECLARED);
        abort();
    }
}
void TypeInferenceAction::visitConditionalOperator(const ConditionalOperatorPtr& node)
{

}
void TypeInferenceAction::visitBinaryOperator(const BinaryOperatorPtr& node)
{
    SemanticNodeVisitor::visitBinaryOperator(node);
    //look for binary function that matches
    OperatorInfo* op = symbolRegistry->getOperator(node->getOperator());
    SymbolPtr sym = symbolRegistry->lookupSymbol(node->getOperator());
    if(!op || !sym)
    {
        error(node, Errors::E_USE_OF_UNRESOLVED_IDENTIFIER, node->getOperator());
        error(node, Errors::E_UNKNOWN_BINARY_OPERATOR, node->getOperator());
        abort();
    }
    if((op->type & OperatorType::InfixBinary) == 0)
    {
        error(node, Errors::E_IS_NOT_BINARY_OPERATOR, node->getOperator());
        error(node, Errors::E_UNKNOWN_BINARY_OPERATOR, node->getOperator());
        abort();
    }
    //find for overload
    FunctionSymbolPtr func = nullptr;
    FunctionOverloadedSymbolPtr overloaded = std::dynamic_pointer_cast<FunctionOverloadedSymbol>(sym);
    if(overloaded)
    {
        TypePtr argv[2];
        argv[0] = node->getLHS()->getType();
        argv[1] = node->getRHS()->getType();
        func = overloaded->lookupOverload(2, argv);
    }
    else
    {
        func = std::dynamic_pointer_cast<FunctionSymbol>(sym);
    }
    if(!func)
    {
        error(node, Errors::E_NO_OVERLOAD_ACCEPTS_ARGUMENTS, node->getOperator());
        abort();
    }
    node->setType(func->getReturnType());
}
void TypeInferenceAction::visitUnaryOperator(const UnaryOperatorPtr& node)
{

}
void TypeInferenceAction::visitTuple(const TuplePtr& node)
{

}
void TypeInferenceAction::visitIdentifier(const IdentifierPtr& node)
{
    SymbolPtr s = symbolRegistry->lookupSymbol(node->getIdentifier());
    if(!s)
    {
        error(node, Errors::E_USE_OF_UNRESOLVED_IDENTIFIER, node->getIdentifier());
        abort();
    }
    TypePtr type = std::dynamic_pointer_cast<Type>(s);
    if(type)
    {
        TypePtr ref = Type::newTypeReference(type);
        node->setType(ref);
    }
    else
    {
        node->setType(s->getType());
    }
}
void TypeInferenceAction::visitCompileConstant(const CompileConstantPtr& node)
{
    const std::wstring& name = node->getName();
    if(name == L"__LINE__" || name == L"__COLUMN__")
    {
        node->setType(t_int);
    }
    else if(name == L"__FUNCTION__" || name == L"__FILE__")
    {
        node->setType(t_string);
    }
    else
    {
        assert(0 && "Invalid compile constant");
        abort();
    }
}



void TypeInferenceAction::visitVariable(const VariablePtr& node)
{
}

void TypeInferenceAction::visitConstant(const ConstantPtr& node)
{
    //TypePtr type = evaluateType(node->initializer);
    if(IdentifierPtr id = std::dynamic_pointer_cast<Identifier>(node->name))
    {
        node->initializer->accept(this);
        TypePtr actualType = node->initializer->getType();
        SymbolPtr sym(new SymbolPlaceHolder(id->getIdentifier(), actualType));

        symbolRegistry->getCurrentScope()->addSymbol(sym);//std::static_pointer_cast<SymboledConstant>(node)
        if(id->getDeclaredType() == NULL)
        {
            id->setType(actualType);
        }
        else
        {
            //TODO check if the type is convertible

        }
    }
    else if(TuplePtr id = std::dynamic_pointer_cast<Tuple>(node->name))
    {
        TypeNodePtr declaredType = id->getDeclaredType();
        if(declaredType)
        {
            checkTupleDefinition(id, node->initializer);
        }
    }
}
TypePtr TypeInferenceAction::evaluateType(const ExpressionPtr& expr)
{
    switch(expr->getNodeType())
    {
        case NodeType::IntegerLiteral:
            return symbolRegistry->lookupType(L"Int");
        case NodeType::FloatLiteral:
            return symbolRegistry->lookupType(L"Float");
        case NodeType::StringLiteral:
            return symbolRegistry->lookupType(L"String");
        default:
            return nullptr;
    }
}