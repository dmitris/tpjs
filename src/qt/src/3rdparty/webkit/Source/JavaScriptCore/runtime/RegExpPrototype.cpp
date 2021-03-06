/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*
 * Portions of this code are Copyright (C) 2014 Yahoo! Inc. Licensed 
 * under the LGPL license.
 * 
 * Author: Nera Liu <neraliu@yahoo-inc.com>
 *
 */
#include "config.h"
#include "RegExpPrototype.h"

#ifdef JSC_TAINTED
#include "JSArray.h"
#include "ArrayConstructor.h"
#endif

#include "ArrayPrototype.h"
#include "Error.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSObject.h"
#include "JSString.h"
#include "JSStringBuilder.h"
#include "JSValue.h"
#include "ObjectPrototype.h"
#include "RegExpObject.h"
#include "RegExp.h"
#include "RegExpCache.h"
#include "StringRecursionChecker.h"
#include "UStringConcatenate.h"
#include "TaintedCounter.h"
#include "TaintedTrace.h"
#include <sstream>

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(RegExpPrototype);

static EncodedJSValue JSC_HOST_CALL regExpProtoFuncTest(ExecState*);
static EncodedJSValue JSC_HOST_CALL regExpProtoFuncExec(ExecState*);
static EncodedJSValue JSC_HOST_CALL regExpProtoFuncCompile(ExecState*);
static EncodedJSValue JSC_HOST_CALL regExpProtoFuncToString(ExecState*);

// ECMA 15.10.5

RegExpPrototype::RegExpPrototype(ExecState* exec, JSGlobalObject* globalObject, Structure* structure, Structure* functionStructure)
    : RegExpObject(globalObject, structure, RegExp::create(&exec->globalData(), "", NoFlags))
{
    putDirectFunctionWithoutTransition(exec, new (exec) JSFunction(exec, globalObject, functionStructure, 2, exec->propertyNames().compile, regExpProtoFuncCompile), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) JSFunction(exec, globalObject, functionStructure, 1, exec->propertyNames().exec, regExpProtoFuncExec), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) JSFunction(exec, globalObject, functionStructure, 1, exec->propertyNames().test, regExpProtoFuncTest), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) JSFunction(exec, globalObject, functionStructure, 0, exec->propertyNames().toString, regExpProtoFuncToString), DontEnum);
}

// ------------------------------ Functions ---------------------------

EncodedJSValue JSC_HOST_CALL regExpProtoFuncTest(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    if (!thisValue.inherits(&RegExpObject::s_info))
        return throwVMTypeError(exec);
    return JSValue::encode(asRegExpObject(thisValue)->test(exec));
}

EncodedJSValue JSC_HOST_CALL regExpProtoFuncExec(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    if (!thisValue.inherits(&RegExpObject::s_info))
        return throwVMTypeError(exec);
#ifdef JSC_TAINTED
    JSValue a = asRegExpObject(thisValue)->exec(exec);
    if (a.inherits(&JSArray::s_info)) {

	unsigned int tainted = 0;
	JSValue s = exec->argument(0);
	if (s.isString() && s.isTainted()) {
		tainted = s.isTainted();
	}
	if (s.inherits(&StringObject::s_info) && asStringObject(s)->isTainted()) {
		tainted = asStringObject(s)->isTainted();
	}
	if (s.isObject()) {
		UString str = s.toString(exec);
		if (str.isTainted()) {
			tainted = s.isTainted();
		}
    	}

	if (tainted) {
	    TaintedStructure trace_struct;
	    trace_struct.taintedno = tainted;
            trace_struct.internalfunc = "regExpProtoFuncExec";
            trace_struct.jsfunc = "RegExp.exec";
	    trace_struct.action = "propagate";

	    char msg[20];
            stringstream msgss;
            snprintf(msg, 20, "%s", s.toString(exec).utf8(true).data());
            msgss << msg;
            msgss >> trace_struct.value;

	    TaintedTrace* trace = TaintedTrace::getInstance();
	    trace->addTaintedTrace(trace_struct);
	}
#ifdef JSC_TAINTED_DEBUG
std::cerr << "regExpProtoFuncExec:" << tainted << std::endl;
#endif

        JSArray* resObj = constructEmptyArray(exec);
	JSObject* thisObj = a.toThisObject(exec);

	unsigned length = asArray(a)->get(exec, exec->propertyNames().length).toUInt32(exec);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());

	unsigned n = 0;
	for (unsigned k = 0; k < length; k++, n++) {
	    PropertySlot slot(thisObj);
	    if (!thisObj->getPropertySlot(exec, k, slot)) {
		JSValue val = JSValue();
            	resObj->put(exec, n, val);
	    } else {
    		JSValue val = slot.getValue(exec, k);
		if (tainted && val.isString()) { val.setTainted(tainted);
		} else if (!tainted && val.isString()) { val.setTainted(tainted); }
		if (tainted && val.inherits(&StringObject::s_info)) { asStringObject(val)->setTainted(tainted);
		} else if (!tainted && val.inherits(&StringObject::s_info)) { asStringObject(val)->setTainted(tainted); }
		resObj->put(exec, n, val);
	    }
    	}
	resObj->setLength(n);

        JSValue result = resObj;
	return JSValue::encode(result);
    }
    return JSValue::encode(jsUndefined());
#else
    return JSValue::encode(asRegExpObject(thisValue)->exec(exec));
#endif
}

EncodedJSValue JSC_HOST_CALL regExpProtoFuncCompile(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    if (!thisValue.inherits(&RegExpObject::s_info))
        return throwVMTypeError(exec);

    RefPtr<RegExp> regExp;
    JSValue arg0 = exec->argument(0);
    JSValue arg1 = exec->argument(1);
    
    if (arg0.inherits(&RegExpObject::s_info)) {
        if (!arg1.isUndefined())
            return throwVMError(exec, createTypeError(exec, "Cannot supply flags when constructing one RegExp from another."));
        regExp = asRegExpObject(arg0)->regExp();
    } else {
        UString pattern = !exec->argumentCount() ? UString("") : arg0.toString(exec);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());

        RegExpFlags flags = NoFlags;
        if (!arg1.isUndefined()) {
            flags = regExpFlags(arg1.toString(exec));
            if (exec->hadException())
                return JSValue::encode(jsUndefined());
            if (flags == InvalidFlags)
                return throwVMError(exec, createSyntaxError(exec, "Invalid flags supplied to RegExp constructor."));
        }
        regExp = exec->globalData().regExpCache()->lookupOrCreate(pattern, flags);
    }

    if (!regExp->isValid())
        return throwVMError(exec, createSyntaxError(exec, regExp->errorMessage()));

    asRegExpObject(thisValue)->setRegExp(regExp.release());
    asRegExpObject(thisValue)->setLastIndex(0);
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL regExpProtoFuncToString(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    if (!thisValue.inherits(&RegExpObject::s_info)) {
        if (thisValue.inherits(&RegExpPrototype::s_info))
            return JSValue::encode(jsNontrivialString(exec, "//"));
        return throwVMTypeError(exec);
    }

    RegExpObject* thisObject = asRegExpObject(thisValue);

    StringRecursionChecker checker(exec, thisObject);
    if (EncodedJSValue earlyReturnValue = checker.earlyReturnValue())
        return earlyReturnValue;

    char postfix[5] = { '/', 0, 0, 0, 0 };
    int index = 1;
    if (thisObject->get(exec, exec->propertyNames().global).toBoolean(exec))
        postfix[index++] = 'g';
    if (thisObject->get(exec, exec->propertyNames().ignoreCase).toBoolean(exec))
        postfix[index++] = 'i';
    if (thisObject->get(exec, exec->propertyNames().multiline).toBoolean(exec))
        postfix[index] = 'm';
    UString source = thisObject->get(exec, exec->propertyNames().source).toString(exec);
    // If source is empty, use "/(?:)/" to avoid colliding with comment syntax
    return JSValue::encode(jsMakeNontrivialString(exec, "/", source.length() ? source : UString("(?:)"), postfix));
}

} // namespace JSC
