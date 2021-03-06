
#include <limits>

#include "compiler/model.h"
#include "core/parser.h"
#include "core/interpreter/blockexecution.h"
#include "core/interpreter/evaluator.h"
#include "core/interpreter/programloader.h"
#include "core/util/unused.h"

//#define EXECUTION_TRACE 1

BlockExecution::BlockExecution(Program block,
                               const Evaluator &evaluator,
                               const Variable& scope,
                               Object *object)
    : program(block),
      begin(block.begin()),
      end(block.end()),
      current(block.begin()),
      last(block.end()),
      lineRepeatCount(0),
      eval(evaluator),
      scope(scope),
      _object(object),
      _inLoop(false),
      _subBlockExitCode(ExitCode::NoExit)
{
    UNUSED(hmcElemNames);
}

BlockExecution::ExitCode BlockExecution::execute()
{
    size_t parseQuota = std::numeric_limits<size_t>::max();
    return execute(end, parseQuota);
}

BlockExecution::ExitCode BlockExecution::execute(Program::const_iterator breakpoint)
{
    size_t parseQuota = std::numeric_limits<size_t>::max();
    return execute(breakpoint, parseQuota);
}

BlockExecution::ExitCode BlockExecution::execute(size_t &parseQuota)
{
    return execute(end, parseQuota);
}

BlockExecution::ExitCode BlockExecution::execute(Program::const_iterator breakpoint, size_t &parseQuota)
{

    while(current != end && current != breakpoint && parseQuota > 0)
    {
        if(current!=last) {
            lineRepeatCount = 0;
        }
        last = current;

        const Program& line = *current;
        if(subBlock)
        {
            if(subBlock->done())
            {
                if(_subBlockExitCode == ExitCode::Returned)
                {
                    _returnValue = subBlock->returnValue();
                    resetSubBlock();
                    return ExitCode::Returned;
                }
                else
                {
                    resetSubBlock();
                    switch(line.tag())
                    {

                        case HMC_LOOP:
                        case HMC_DO_LOOP:
                            if(_subBlockExitCode == ExitCode::Broken)
                            {
                                ++current;
                            }
                            break;

                        default:
                            if(_subBlockExitCode == ExitCode::Broken && handleBreak())
                            {
                                return ExitCode::Broken;
                            }

                            if(_subBlockExitCode == ExitCode::Continued && handleContinue())
                            {
                                return ExitCode::Continued;
                            }

                            ++current;
                            break;
                    }
                }
            }
            else
            {
                _subBlockExitCode = subBlock->execute(parseQuota);
            }
        }
        else
        {
            ++lineRepeatCount;
            switch(line.tag())
            {
                case HMC_DECLARATION:
                    handleDeclaration(line, parseQuota);
                    break;

                case HMC_LOCAL_DECLARATIONS:
                    handleLocalDeclarations(line);
                    break;

                case HMC_REMOVE:
                    handleRemove(line);
                    break;

                case HMC_RIGHT_VALUE:
                    handleRightValue(line);
                    break;

                case HMC_CONDITIONAL_STATEMENT:
                    handleCondition(line);
                    break;

                case HMC_LOOP:
                    handleLoop(line);
                    break;

                case HMC_DO_LOOP:
                    handleDoLoop(line);
                    break;

                case HMC_BREAK:
                if(handleBreak())
                    return ExitCode::Broken;

                case HMC_CONTINUE:
                if(handleContinue())
                    return ExitCode::Continued;

                case HMC_RETURN:
                    handleReturn(line);
                    return ExitCode::Returned;

                default:
                    ++current;
                    break;
            }
        }
    }
    if(current == end)
        return ExitCode::EndReached;

    if(current == breakpoint)
        return ExitCode::BreakPointReached;

    return ExitCode::QuotaExhausted;
}

bool BlockExecution::done()
{
    return current == end;
}

Variable BlockExecution::returnValue()
{
    return _returnValue;
}

VariableCollector &BlockExecution::collector() const
{
    return scope.collector();
}

void BlockExecution::setSubBlock(Program program, bool loop)
{
    subBlock.reset(new BlockExecution(program, eval, scope, _object));
    if(loop || _inLoop)
        subBlock->_inLoop = true;
    _subBlockExitCode = ExitCode::NoExit;
}

void BlockExecution::resetSubBlock()
{
    subBlock.reset();
}

void BlockExecution::handleDeclaration(const Program &declaration, size_t &parseQuota)
{
    if(_object != nullptr)
    {
        const Variant typeValue = eval.rightValue(declaration.node(0)).value();

        if (!typeValue.isValueless()) {
            const ObjectType type = typeValue.toObjectType();

            const Program& nameProgram = declaration.node(1);

            std::string name = nameProgram.tag() == HMC_IDENTIFIER ?
                                   nameProgram.payload().toString()
                                 : eval.rightValue(nameProgram).value().toString();
#ifdef EXECUTION_TRACE
            std::stringstream S;
            S<<"Declaration "<<type<<" "<<name;
            std::cerr<<S.str()<<std::endl;
#endif
            if (_object->addVariable(type, name) != nullptr) {
                --parseQuota;
            }
        }
    }
    ++current;
}

void BlockExecution::handleLocalDeclarations(const Program &declarations)
{
    for (const Program& declaration : declarations) {

        Variable value;
        if (declaration.size() >= 2) {
            value = collector().copy(eval.rightValue(declaration.node(1)).value());
        } else {
            value = collector().null();
        }

        scope.setField(declaration.node(0).payload(), value);

#ifdef EXECUTION_TRACE
        std::stringstream S;
        S << "Local declaration "<<declaration.node(0).payload()<<" = "<<value.value();
        std::cerr<<S.str()<<std::endl;
#endif
    }
    ++current;
}

void BlockExecution::handleRemove(const Program &remove)
{
    scope.removeField(eval.variablePath(remove.node(0)));

    ++current;
}

void BlockExecution::handleRightValue(const Program &rightValue)
{
#ifdef EXECUTION_TRACE
    Variant value = eval.rightValue(rightValue).value();
#else
    eval.rightValue(rightValue);
#endif
#ifdef EXECUTION_TRACE
    std::stringstream S;
    S<<"Right value "<<value;
    std::cerr<<S.str()<<std::endl;
#endif
    ++current;
}

void BlockExecution::handleCondition(const Program &condition)
{
#ifdef EXECUTION_TRACE
    std::cerr<<"Condition"<<std::endl;
#endif
    if(eval.rightValue(condition.node(0)).value().toBool())
    {
#ifdef EXECUTION_TRACE
        std::cerr<<" then"<<std::endl;
#endif
        setSubBlock(condition.node(1), false);
    }
    else
    {
#ifdef EXECUTION_TRACE
        std::cerr<<" else"<<std::endl;
#endif
        setSubBlock(condition.node(2), false);
    }
#ifdef EXECUTION_TRACE
    std::cerr<<std::endl;
#endif
}

void BlockExecution::handleLoop(const Program &loop)
{
#ifdef EXECUTION_TRACE
    std::cerr<<"Loop"<<std::endl;
#endif
    if(loopCondition(loop))
    {
#ifdef EXECUTION_TRACE
        std::cerr<<" continue"<<std::endl;
#endif
        setSubBlock(loop.node(1), true);
    }
    else
    {
#ifdef EXECUTION_TRACE
        std::cerr<<" done"<<std::endl;
#endif
        ++current;
    }
#ifdef EXECUTION_TRACE
    std::cerr<<std::endl;
#endif

}

void BlockExecution::handleDoLoop(const Program &loop)
{
#ifdef EXECUTION_TRACE
    std::cerr<<"Do Loop : repeat count "<<lineRepeatCount<<std::endl;
#endif
    if(lineRepeatCount <= 1 || loopCondition(loop))
    {
#ifdef EXECUTION_TRACE
        std::cerr<<" continue"<<std::endl;
#endif
        setSubBlock(loop.node(1), true);
    }
    else
    {
#ifdef EXECUTION_TRACE
        std::cerr<<" done"<<std::endl;
#endif
        ++current;
    }
#ifdef EXECUTION_TRACE
    std::cerr<<std::endl;
#endif
}

bool BlockExecution::handleBreak()
{
    if(_inLoop)
    {
#ifdef EXECUTION_TRACE
        std::cerr<<"Break"<<std::endl;
#endif
        current = end;
        return true;
    }
    ++current;
    return false;
}

bool BlockExecution::handleContinue()
{
    if(_inLoop)
    {
#ifdef EXECUTION_TRACE
        std::cerr<<"Continue"<<std::endl;
#endif
        current = end;
        return true;
    }
    ++current;
    return false;
}

void BlockExecution::handleReturn(const Program &line)
{
    const Program& rightValue = line.node(0);
    _returnValue = eval.rightValue(rightValue);
#ifdef EXECUTION_TRACE
    std::cerr<<"Return "<<_returnValue.value()<<std::endl;
#endif
    current = end;
}

bool BlockExecution::loopCondition(const Program &loop)
{
    Variable variable = eval.rightValue(loop.node(0));
    bool check = variable.value().toBool();
    return check
            && ((!hasDeclaration(loop.node(1)) || (_object != nullptr && _object->availableSize()!= 0)));
}

bool BlockExecution::hasDeclaration(const Program &instructions)
{
    for(Program program: instructions)
    {
        switch(program.tag())
        {
            case HMC_DECLARATION:
                return true;

            case HMC_CONDITIONAL_STATEMENT:
                if(hasDeclaration(program.node(1)))
                    return true;
                if(hasDeclaration(program.node(2)))
                    return true;
                break;

            case HMC_LOOP:
            case HMC_DO_LOOP:
                if(hasDeclaration(program.node(1)))
                    return true;
                break;

            default:
                break;
        }
    }
    return false;
}

