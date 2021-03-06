//This file is part of the HexaMonkey project, a multimedia analyser
//Copyright (C) 2013  Sevan Drapeau-Martin, Nicolas Fleury

//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either version 2
//of the License, or (at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "compiler/model.h"
#include "core/interpreter/fromfilemodule.h"
#include "core/interpreter/fromfiletemplate.h"
#include "core/interpreter/programloader.h"
#include "core/interpreter/fromfiletemplate.h"
#include "core/interpreter/fromfilemethod.h"
#include "core/variable/variablepath.h"
#include "core/variable/variable.h"
#include "core/interpreter/fromfileparser.h"
#include "core/interpreter/blockexecution.h"
#include "core/variable/localscope.h"
#include "core/variable/typescope.h"
#include "core/variable/variablecollector.h"
#include "core/util/fileutil.h"
#include "core/util/unused.h"

//#define LOAD_TRACE 1

const VariablePath sizeDescriptor = {"@size"};
const std::vector<VariablePath> headerOnlyVars = {
    sizeDescriptor,
    {"@args"},
    {"@value"},
    {"@info"},
    {"@linkTo"},
    {"@attr"}
};

const std::vector<std::string> emptyParameterNames;
const std::vector<bool> emptyParameterModifiables;
const std::vector<Variant> emptyParameterDefaults;

FromFileModule::FromFileModule(Program program)
    : _program(program),
      _scope(_collector.null()),
      _evaluator(_scope, *this)
{
    UNUSED(hmcElemNames);
}

void FromFileModule::addFormatDetection(StandardFormatDetector::Adder &formatAdder)
{
    if(program().isValid())
    {
        Program formatDetections = program().node(0);
        loadFormatDetections(formatDetections, formatAdder);
    }
}

void FromFileModule::requestImportations(std::vector<std::string> &formatRequested)
{
    if(program().isValid())
    {
        Program imports = program().node(1);
        loadImports(imports, formatRequested);
    }
}

bool FromFileModule::doLoad()
{
    if(!program().isValid())
        return false;

    Program classDeclarations = program().node(2);
    nameScan(classDeclarations);
    loadSpecifications(classDeclarations);
    return true;
}

void FromFileModule::loadFormatDetections(Program &formatDetections, StandardFormatDetector::Adder &formatAdder)
{
    for(Program formatDetection: formatDetections)
    {
        switch(formatDetection.node(0).payload().toInteger())
        {
            case HMC_ADD_MAGIC_NUMBER_OP:
                formatAdder.addMagicNumber(formatDetection.node(1).payload().toString());
                break;

            case HMC_ADD_EXTENSION_OP:
                formatAdder.addExtension(formatDetection.node(1).payload().toString());
                break;

            case HMC_ADD_SYNCBYTE_OP:
                formatAdder.addSyncbyte(formatDetection.node(1).payload().toInteger(), formatDetection.node(2).payload().toInteger());
                break;

            default:
                break;
        }
    }
}

void FromFileModule::loadImports(Program &imports, std::vector<std::string> &formatRequested)
{
    for(Program module: imports)
    {
        formatRequested.push_back(module.payload().toString());
    }
}

std::unordered_map<std::string, ObjectTypeTemplate::Attribute> _attributes = {
    {"elemType", ObjectTypeTemplate::Attribute::elementType},
    {"elemCount", ObjectTypeTemplate::Attribute::elementCount},
    {"name", ObjectTypeTemplate::Attribute::name},
    {"displayMode", ObjectTypeTemplate::Attribute::displayMode},
    {"displayAs", ObjectTypeTemplate::Attribute::displayAs}
};

void FromFileModule::nameScan(Program& declarations)
{
#ifdef LOAD_TRACE
    std::cerr<<"Load templates :"<<std::endl;
#endif

    for(Program declaration : declarations)
    {
        if(declaration.tag() == HMC_CLASS_DECLARATION)
        {
            ObjectTypeTemplate& objectTypeTemplate = addTemplate(new FromFileTemplate(declaration, *this, _collector, _evaluator));
#ifdef LOAD_TRACE
            std::cerr<<"    "<<objectTypeTemplate<<std::endl;
#endif
        }
        else if(declaration.tag() == HMC_FUNCTION_DECLARATION)
        {
            const std::string& name = declaration.node(0).payload().toString();
            const Program& arguments = declaration.node(1);
            const Program& definition = declaration.node(2);

            std::vector<MethodArgument> signature;

            for(const Program& argument: arguments)
            {
                signature.emplace_back(MethodArgument(argument.node(1).payload().toString(), !argument.node(0).payload().toBool(), _evaluator.rightValue(argument.node(2)).value()));
            }
            addMethod(name, new FromFileMethod(definition, signature, *this));
        }
    }
#ifdef LOAD_TRACE
    std::cerr<<std::endl;
#endif
}

void FromFileModule::loadSpecifications(Program &classDeclarations)
{
#ifdef LOAD_TRACE
    std::cerr<<"Load specifications :"<<std::endl;
#endif

    VariableCollectionGuard guard(_collector);

    for(Program classDeclaration : classDeclarations)
    {
        if(classDeclaration.tag() == HMC_CLASS_DECLARATION)
        {
            ObjectType child = getType(classDeclaration.node(0).node(0).node(0).payload().toString());
            for(Program type : classDeclaration.node(0).node(2))
            {
                ObjectType parent(_evaluator.type(type));
                setSpecification(parent, child);
#ifdef LOAD_TRACE
        std::cerr<<"    "<<child<<" specifies "<<parent<<std::endl;
#endif
            }
        }
        else if(classDeclaration.tag() == HMC_FORWARD)
        {
            ObjectType parent(_evaluator.type(classDeclaration.node(0)));
            ObjectType child(_evaluator.type(classDeclaration.node(1)));
            setSpecification(parent, child);
#ifdef LOAD_TRACE
        std::cerr<<"    "<<child<<" specifies "<<parent<<std::endl;
#endif
        }
    }
#ifdef LOAD_TRACE
    std::cerr<<std::endl;
#endif
}


