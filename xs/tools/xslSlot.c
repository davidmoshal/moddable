/*
 * Copyright (c) 2016-2018  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK Tools.
 * 
 *   The Moddable SDK Tools is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 * 
 *   The Moddable SDK Tools is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 * 
 *   You should have received a copy of the GNU General Public License
 *   along with the Moddable SDK Tools.  If not, see <http://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and  
 * permission notice:  
 *
 *       Copyright (C) 2010-2016 Marvell International Ltd.
 *       Copyright (C) 2002-2010 Kinoma, Inc.
 *
 *       Licensed under the Apache License, Version 2.0 (the "License");
 *       you may not use this file except in compliance with the License.
 *       You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *       Unless required by applicable law or agreed to in writing, software
 *       distributed under the License is distributed on an "AS IS" BASIS,
 *       WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *       See the License for the specific language governing permissions and
 *       limitations under the License.
 */

#include "xsl.h"

static txString fxGetBuilderName(txMachine* the, const txHostFunctionBuilder* which);
static txString fxGetCallbackName(txMachine* the, txCallback callback); 
static txString fxGetCodeName(txMachine* the, txByte* which);
static txInteger fxGetTypeDispatchIndex(txTypeDispatch* dispatch);
static void fxPrepareInstance(txMachine* the, txSlot* instance);
static void fxPrintAddress(txMachine* the, FILE* file, txSlot* slot);
static void fxPrintID(txMachine* the, FILE* file, txID id);
static void fxPrintNumber(txMachine* the, FILE* file, txNumber value);
static void fxPrintSlot(txMachine* the, FILE* file, txSlot* slot, txFlag flag);

txSlot* fxBuildHostConstructor(txMachine* the, txCallback call, txInteger length, txInteger id)
{
	txLinker* linker = (txLinker*)(the->context);
	fxNewLinkerBuilder(linker, call, length, id);
	return fxNewHostConstructor(the, call, length, id);
}

txSlot* fxBuildHostFunction(txMachine* the, txCallback call, txInteger length, txInteger id)
{
	txLinker* linker = (txLinker*)(the->context);
	fxNewLinkerBuilder(linker, call, length, id);
	return fxNewHostFunction(the, call, length, id);
}

txString fxGetBuilderName(txMachine* the, const txHostFunctionBuilder* which) 
{
	txLinker* linker = xsGetContext(the);
	static char buffer[1024];
	const txHostFunctionBuilder* builder;
	{
		txLinkerBuilder* linkerBuilder = linker->firstBuilder;
		while (linkerBuilder) {
			builder = &(linkerBuilder->host);
			if (builder == which) {
				sprintf(buffer, "&gxBuilders[%d]", linkerBuilder->builderIndex);
				return buffer;
			}
			linkerBuilder = linkerBuilder->nextBuilder;
		}
	}
	{
		txLinkerScript* linkerScript = linker->firstScript;
		while (linkerScript) {
			txInteger count = linkerScript->hostsCount;
			if (count) {
				txInteger index;
				for (builder = linkerScript->builders, index = 0; index < count; builder++, index++) {
					if (builder == which) {
						sprintf(buffer, "&gxBuilders%d[%d]", linkerScript->scriptIndex, index);
						return buffer;
					}
				}
			}
			linkerScript = linkerScript->nextScript;
		}
	}
	return "OOPS";
}

txString fxGetCallbackName(txMachine* the, txCallback which) 
{
	txLinker* linker = xsGetContext(the);
	if (!which)
		return "NULL";
	{
		txLinkerCallback* linkerCallback = fxGetLinkerCallbackByAddress(linker, which);
		if (linkerCallback) {
			if (linkerCallback->flag)
				return linkerCallback->name;
			return "fxDeadStrip";
		}
	}
	{
		txLinkerScript* linkerScript = linker->firstScript;
		while (linkerScript) {
			txInteger count = linkerScript->hostsCount;
			if (count) {
				txCallbackName* address = &(linkerScript->callbackNames[0]);
				while (count) {
					if (address->callback == which)
						return address->name;
					address++;
					count--;
				}
			}
			linkerScript = linkerScript->nextScript;
		}
	}
	return "OOPS";
}

txString fxGetCodeName(txMachine* the, txByte* which) 
{
	if (which == gxNoCode)
		return "(txByte*)(gxNoCode)";
	{
		static char buffer[1024];
		txLinker* linker = xsGetContext(the);
		txLinkerScript* linkerScript = linker->firstScript;
		while (linkerScript) {
			txS1* from = linkerScript->codeBuffer;
			txS1* to = from + linkerScript->codeSize;
			if ((from <= which) && (which < to)) {
				sprintf(buffer, "(txByte*)(&gxCode%d[%ld])", linkerScript->scriptIndex, (long)(which - from));
				return buffer;
			}
			linkerScript = linkerScript->nextScript;
		}
	}
	return "OOPS";
}

extern const txTypeDispatch gxTypeDispatches[];
txInteger fxGetTypeDispatchIndex(txTypeDispatch* dispatch) 
{
	txInteger i = 0;
	while (i < mxTypeArrayCount) {
		if (dispatch == &gxTypeDispatches[i])
			return i;
		i++;
	}
	fprintf(stderr, "dispatch %p %p\n", dispatch, &gxTypeDispatches[0]);
	return -1;
}

extern const txTypeAtomics gxTypeAtomics[];
txInteger fxGetTypeAtomicsIndex(txTypeAtomics* atomics) 
{
	txInteger i = 0;
	while (i < mxTypeArrayCount) {
		if (atomics == &gxTypeAtomics[i])
			return i;
		i++;
	}
	fprintf(stderr, "atomics %p %p\n", atomics, &gxTypeAtomics[0]);
	return -1;
}

void fxLinkerScriptCallback(txMachine* the)
{
	txLinker* linker = xsGetContext(the);
    txSlot* slot = mxModuleInternal(mxThis);
    txSlot* key = fxGetKey(the, slot->value.module.id);
    txString path = key->value.key.string + linker->baseLength;
	txLinkerScript* linkerScript = linker->firstScript;
	mxPush(mxArrayPrototype);
	fxNewArrayInstance(the);
	fxArrayCacheBegin(the, the->stack);
	while (linkerScript) {
		if (!c_strcmp(linkerScript->path, path)) {
			txByte* p = linkerScript->hostsBuffer;
			if (p) {
				txID c, i, id;
				mxDecode2(p, c);
				linkerScript->builders = fxNewLinkerChunk(linker, c * sizeof(txHostFunctionBuilder));
				linkerScript->callbackNames = fxNewLinkerChunk(linker, c * sizeof(txCallbackName));
				linkerScript->hostsCount = c;
				for (i = 0; i < c; i++) {
					txCallback callback = the->fakeCallback;
					txHostFunctionBuilder* builder = &linkerScript->builders[i];
					txCallbackName* callbackName = &linkerScript->callbackNames[i];
					txS1 length = *p++;
					the->fakeCallback = (txCallback)(((txU1*)the->fakeCallback) + 1);
					mxDecode2(p, id);
					builder->callback = callback;
					builder->length = length;
					builder->id = id;
					callbackName->callback = callback;
					callbackName->name = (char*)p;
					if (length < 0) {
						fxNewHostObject(the, (txDestructor)callback);
					}
					else {
						mxPushUndefined();
						the->stack->kind = XS_HOST_FUNCTION_KIND;
						the->stack->value.hostFunction.builder = builder;
						the->stack->value.hostFunction.IDs = NULL;
					}
					fxArrayCacheItem(the, the->stack + 1, the->stack);
					the->stack++;
					p += c_strlen((char*)p) + 1;
				}
			}
			break;
		}	
		linkerScript = linkerScript->nextScript;
	}
	fxArrayCacheEnd(the, the->stack);
	mxPullSlot(mxResult);
	mxPop();
}

txSlot* fxNewFunctionLength(txMachine* the, txSlot* instance, txSlot* property, txInteger length)
{
	txLinker* linker = (txLinker*)(the->context);
	if (linker->stripping)
		return property;
	property = property->next = fxNewSlot(the);
	property->flag = XS_DONT_ENUM_FLAG | XS_DONT_SET_FLAG;
	property->ID = mxID(_length);
	property->kind = XS_INTEGER_KIND;
	property->value.integer = length;
	return property;
}

txSlot* fxNewFunctionName(txMachine* the, txSlot* instance, txInteger id, txInteger former, txString prefix)
{
	txSlot* property;
	txSlot* key;
	txLinker* linker = (txLinker*)(the->context);
	if (linker->stripping)
		return C_NULL;
	property = mxBehaviorGetProperty(the, instance, mxID(_name), XS_NO_ID, XS_OWN);
	if (property)
		return property;
	property = fxNextSlotProperty(the, fxLastProperty(the, instance), &mxEmptyString, mxID(_name), XS_DONT_ENUM_FLAG | XS_DONT_SET_FLAG);
	key = fxGetKey(the, (txID)id);
	if (key) {
		txKind kind = mxGetKeySlotKind(key);
		if (kind == XS_KEY_KIND) {
			property->kind = XS_STRING_KIND;
			property->value.string = key->value.key.string;
		}
		else if (kind == XS_KEY_X_KIND) {
			property->kind = XS_STRING_X_KIND;
			property->value.string = key->value.key.string;
		}
		else if ((kind == XS_STRING_KIND) || (kind == XS_STRING_X_KIND)) {
			property->kind = kind;
			property->value.string = key->value.string;
			fxAdornStringC(the, "[", property, "]");
		}
		else {
			property->kind = mxEmptyString.kind;
			property->value = mxEmptyString.value;
		}
	}
	else {
		property->kind = mxEmptyString.kind;
		property->value = mxEmptyString.value;
	}
	if (prefix) 
		fxAdornStringC(the, prefix, property, C_NULL);
	return property;
}

txSlot* fxNextHostFunctionProperty(txMachine* the, txSlot* property, txCallback call, txInteger length, txID id, txFlag flag)
{
	txLinker* linker = (txLinker*)(the->context);
	txSlot *function, *home = the->stack, *slot;
	if (linker->stripping) {
		property = property->next = fxNewSlot(the);
		property->flag = flag;
		property->ID = id;
		property->kind = XS_HOST_FUNCTION_KIND;
		property->value.hostFunction.builder = fxNewLinkerBuilder(linker, call, length, id);
		property->value.hostFunction.IDs = (txID*)the->code;
	}
	else {
		function = fxNewHostFunction(the, call, length, id);
		slot = mxFunctionInstanceHome(function);
		slot->value.home.object = home->value.reference;
		property = property->next = fxNewSlot(the);
		property->flag = flag;
		property->ID = id;
		property->kind = the->stack->kind;
		property->value = the->stack->value;
		the->stack++;
	}
	return property;
}

void fxPrepareInstance(txMachine* the, txSlot* instance)
{
	txLinker* linker = (txLinker*)(the->context);
	if (linker->stripping) {
		txSlot *property = instance->next;
		while (property) {
			if (property->kind != XS_ACCESSOR_KIND) 
				property->flag |= XS_DONT_SET_FLAG;
			property->flag |= XS_DONT_DELETE_FLAG;
			property = property->next;
		}
		instance->flag |= XS_DONT_PATCH_FLAG;
	}
}

txInteger fxPrepareHeap(txMachine* the, txBoolean stripping)
{
	txLinker* linker = (txLinker*)(the->context);
	txID aliasCount = 0;
	txInteger index = 1;
	txSlot *heap, *slot, *limit, *item;
	txLinkerProjection* projection;
	txSlot* home = NULL;

	heap = the->firstHeap;
	while (heap) {
		slot = heap + 1;
		limit = heap->value.reference;
		while (slot < limit) {
			txSlot* next = slot->next;
			if (next && (next->next == NULL) && (next->ID == XS_NO_ID) && (next->kind == XS_HOME_KIND)) {
				if (home && (home->flag == next->flag) && (home->value.home.object == next->value.home.object) && (home->value.home.module == next->value.home.module))
					slot->next = home;
				else
					home = next;
			}
			slot++;
		}
		heap = heap->next;
	}
	xsCollectGarbage();

	slot = the->freeHeap;
	while (slot) {
		slot->flag |= XS_MARK_FLAG;
		slot = slot->next;
	}
	
	heap = the->firstHeap;
	while (heap) {
		slot = heap + 1;
		limit = heap->value.reference;
		projection = fxNewLinkerChunkClear(linker, sizeof(txLinkerProjection) + (limit - slot) * sizeof(txInteger));
		projection->nextProjection = linker->firstProjection;
		projection->heap = heap;
		projection->limit = limit;
		linker->firstProjection = projection;
		while (slot < limit) {
			if (!(slot->flag & XS_MARK_FLAG)) {
				projection->indexes[slot - heap] = index;
				index++;
				if ((slot->kind == XS_ARRAY_KIND) && ((item = slot->value.array.address))) {
					txInteger size = (txInteger)fxGetIndexSize(the, slot);
					index++;
					while (size) {
						index++;
						if (item->kind != XS_ACCESSOR_KIND) 
							item->flag |= XS_DONT_SET_FLAG;
						item->flag |= XS_DONT_DELETE_FLAG;
						item++;
						size--;
					}
					slot->flag |= XS_DONT_DELETE_FLAG | XS_DONT_SET_FLAG;
				}
				else if (slot->kind == XS_INSTANCE_KIND) {
					txSlot *property = slot->next;
					if (property) {
						if ((property->kind == XS_ARRAY_KIND) && (slot != mxArrayPrototype.value.reference))
							fxPrepareInstance(the, slot);
						else if ((property->kind == XS_CALLBACK_KIND) || (property->kind == XS_CALLBACK_X_KIND) || (property->kind == XS_CODE_KIND) || (property->kind == XS_CODE_X_KIND)) {
							fxPrepareInstance(the, slot);
							if (stripping) {
								if (slot->flag & XS_CAN_CONSTRUCT_FLAG /*(XS_BASE_FLAG | XS_DERIVED_FLAG)*/) {
									property = property->next;
									while (property) {
										if ((property->ID == mxID(_prototype)) && (property->kind == XS_REFERENCE_KIND)) {
											fxPrepareInstance(the, property->value.reference);
											break;
										}
										property = property->next;
									}
								}
							}
						}
						else if (property->kind == XS_MODULE_KIND)
							fxPrepareInstance(the, slot);
						else if (property->kind == XS_EXPORT_KIND)
							fxPrepareInstance(the, slot);
						else if ((property->flag & XS_INTERNAL_FLAG) && (property->ID == XS_ENVIRONMENT_BEHAVIOR))
							fxPrepareInstance(the, slot);
					}
				}
				else if (slot->kind == XS_BIGINT_KIND) {
					linker->bigintSize += slot->value.bigint.size;
				}
			}
			slot++;
		}
		heap = heap->next;
	}
	index++;
	
	slot = mxModuleInstanceInternal(mxProgram.value.reference)->value.module.realm;
	slot = mxRealmGlobal(slot)->value.reference;
	slot->flag &= ~XS_DONT_PATCH_FLAG;
	
	heap = the->firstHeap;
	while (heap) {
		slot = heap + 1;
		limit = heap->value.reference;
		while (slot < limit) {
			if (!(slot->flag & XS_MARK_FLAG)) {
				if (slot->kind == XS_INSTANCE_KIND) {
					txBoolean frozen = (slot->flag & XS_DONT_PATCH_FLAG) ? 1 : 0;
					if (frozen) {
						txSlot *property = slot->next;
						while (property) {
							if (property->kind != XS_ACCESSOR_KIND) 
								if (!(property->flag & XS_DONT_SET_FLAG))
									frozen = 0;
							if (!(property->flag & XS_DONT_DELETE_FLAG))
								frozen = 0;
							property = property->next;
						}
					}
					if (frozen)
						slot->ID = XS_NO_ID;
					else
						slot->ID = aliasCount++;
				}
			}
			slot++;
		}
		heap = heap->next;
	}
	
	heap = the->firstHeap;
	while (heap) {
		slot = heap + 1;
		limit = heap->value.reference;
		while (slot < limit) {
			if (!(slot->flag & XS_MARK_FLAG)) {
				if (slot->kind == XS_CLOSURE_KIND) {
					txSlot* closure = slot->value.closure;
					if (closure->kind == XS_REFERENCE_KIND) {
						txSlot* internal = closure->value.reference->next;
						if (internal && ((internal->kind == XS_CALLBACK_KIND) || (internal->kind == XS_CALLBACK_X_KIND) || (internal->kind == XS_CODE_KIND) || (internal->kind == XS_CODE_X_KIND))) {
							closure->flag |= XS_DONT_SET_FLAG;
						}
					}
					if (closure->flag & XS_DONT_SET_FLAG)
						closure->flag |= XS_DONT_DELETE_FLAG;
					else {
						if (closure->ID == XS_NO_ID)
							closure->ID = aliasCount++;
						slot->flag &= ~XS_DONT_SET_FLAG;
					}
				}
			}
			slot++;
		}
		heap = heap->next;
	}
	
	the->aliasCount = aliasCount;
	
	return index;
}

void fxPrintAddress(txMachine* the, FILE* file, txSlot* slot) 
{
	if (slot) {
		txLinker* linker = (txLinker*)(the->context);
		txLinkerProjection* projection = linker->firstProjection;
		while (projection) {
			if ((projection->heap < slot) && (slot < projection->limit)) {
				fprintf(file, "(txSlot*)&gxHeap[%d]", (int)projection->indexes[slot - projection->heap]);
				return;
			}
			projection = projection->nextProjection;
		}
		fprintf(file, "OOPS");
	}
	else
		fprintf(file, "NULL");
}

void fxPrintBuilders(txMachine* the, FILE* file)
{
	txLinker* linker = (txLinker*)(the->context);
	txLinkerBuilder* linkerBuilder = linker->firstBuilder;
	fprintf(file, "static const xsHostBuilder gxBuilders[%d] = {\n", linker->builderCount);
	while (linkerBuilder) {
		txString name = fxGetCallbackName(the, linkerBuilder->host.callback);
		fprintf(file, "\t{ %s, %d, ", name, linkerBuilder->host.length);
		fxPrintID(the, file, linkerBuilder->host.id);
		fprintf(file, " },\n");
		linkerBuilder = linkerBuilder->nextBuilder;
	}
	fprintf(file, "};\n\n");
}

void fxPrintHeap(txMachine* the, FILE* file, txInteger count)
{
	txLinker* linker = (txLinker*)(the->context);
	txLinkerProjection* projection = linker->firstProjection;
	txSlot *slot, *limit, *item;
	txInteger index = 0;
	fprintf(file, "/* %.4d */", index);
	fprintf(file, "\t{ NULL, {.ID = XS_NO_ID, .flag = XS_NO_FLAG, .kind = XS_REFERENCE_KIND}, ");
	fprintf(file, ".value = { .reference = (txSlot*)&gxHeap[%d] } },\n", (int)count);
	index++;
	while (projection) {
		slot = projection->heap + 1;
		limit = projection->limit;
		while (slot < limit) {
			if (!(slot->flag & XS_MARK_FLAG)) {
				fprintf(file, "/* %.4d */", index);
				index++;
				if ((slot->kind == XS_ARRAY_KIND) && ((item = slot->value.array.address))) {
					txInteger size = (txInteger)fxGetIndexSize(the, slot);
					fprintf(file, "\t{ ");
					fxPrintAddress(the, file, slot->next);
					fprintf(file, ", {.ID = ");
					fxPrintID(the, file, slot->ID);
					fprintf(file, ", .flag = 0x%x", slot->flag | XS_MARK_FLAG);
					fprintf(file, ", .kind = XS_ARRAY_KIND}, .value = { .array = { (txSlot*)&gxHeap[%d], %d } }},\n", (int)index + 1, (int)slot->value.array.length);
					fprintf(file, "/* %.4d */", index);
					index++;
					fprintf(file, "\t{ NULL, {.ID = XS_NO_ID, .flag = 0x80, .kind = XS_INTEGER_KIND}, .value = { .integer = %d * sizeof(txSlot) } },\n", (int)size); // fake chunk
					while (size) {
						fprintf(file, "/* %.4d */", index);
						index++;
						item->flag |= XS_DEBUG_FLAG;
						fxPrintSlot(the, file, item, XS_MARK_FLAG);
						item++;
						size--;
					}
				}
				else {
					fxPrintSlot(the, file, slot, XS_MARK_FLAG);
				}
			}
			slot++;
		}
		projection = projection->nextProjection;
	}	
	fprintf(file, "/* %.4d */", index);
	fprintf(file, "\t{ NULL, {.ID = XS_NO_ID, .flag = XS_NO_FLAG, .kind = XS_REFERENCE_KIND}, ");
	fprintf(file, ".value = { .reference = NULL } }");
}

void fxPrintID(txMachine* the, FILE* file, txID id) 
{
	txLinker* linker = (txLinker*)(the->context);
	if (id == XS_NO_ID)
		fprintf(file, "XS_NO_ID");
	else if (id < 0) {
		char* string = fxGetKeyName(the, id);
		if (string) {
			if (fxIsCIdentifier(linker, string))
				fprintf(file, "xsID_%s", string);
			else
				fprintf(file, "%d /* %s */", id, string);
		}
		else
			fprintf(file, "%d /* ? */", id);
	}
	else
		fprintf(file, "%d", id);
}

void fxPrintNumber(txMachine* the, FILE* file, txNumber value) 
{
	switch (c_fpclassify(value)) {
	case C_FP_INFINITE:
		if (value < 0)
			fprintf(file, "-C_INFINITY");
		else
			fprintf(file, "C_INFINITY");
		break;
	case C_FP_NAN:
		fprintf(file, "C_NAN");
		break;
	default:
		fprintf(file, "%.20e", value);
		break;
	}
}

void fxPrintSlot(txMachine* the, FILE* file, txSlot* slot, txFlag flag)
{
	txLinker* linker = (txLinker*)(the->context);
	fprintf(file, "\t{ ");
	if (slot->flag & XS_DEBUG_FLAG) {
		slot->flag &= ~XS_DEBUG_FLAG;
		fprintf(file, "(txSlot*)%ld", (long int)slot->next);
	}
	else
		fxPrintAddress(the, file, slot->next);
	fprintf(file, ", {.ID = ");
	fxPrintID(the, file, slot->ID);
	fprintf(file, ", ");
	if (slot->kind == 	XS_INSTANCE_KIND)
		fprintf(file, ".flag = 0x%x, ", slot->flag | flag | XS_DONT_MARSHALL_FLAG);
	else
		fprintf(file, ".flag = 0x%x, ", slot->flag | flag);
	switch (slot->kind) {
	case XS_UNINITIALIZED_KIND: {
		fprintf(file, ".kind = XS_UNINITIALIZED_KIND}, ");
		fprintf(file, ".value = { .number = 0 } ");
	} break;
	case XS_UNDEFINED_KIND: {
		fprintf(file, ".kind = XS_UNDEFINED_KIND}, ");
		fprintf(file, ".value = { .number = 0 } ");
	} break;
	case XS_NULL_KIND: {
		fprintf(file, ".kind = XS_NULL_KIND}, ");
		fprintf(file, ".value = { .number = 0 } ");
	} break;
	case XS_BOOLEAN_KIND: {
		fprintf(file, ".kind = XS_BOOLEAN_KIND}, ");
		fprintf(file, ".value = { .boolean = %d } ", slot->value.boolean);
	} break;
	case XS_INTEGER_KIND: {
		fprintf(file, ".kind = XS_INTEGER_KIND}, ");
		fprintf(file, ".value = { .integer = %d } ", slot->value.integer);
	} break;
	case XS_NUMBER_KIND: {
		fprintf(file, ".kind = XS_NUMBER_KIND}, ");
		fprintf(file, ".value = { .number = ");
		fxPrintNumber(the, file, slot->value.number);
		fprintf(file, " } ");
	} break;
	case XS_STRING_KIND:
	case XS_STRING_X_KIND: {
		fprintf(file, ".kind = XS_STRING_X_KIND}, ");
		fprintf(file, ".value = { .string = ");
		fxWriteCString(file, slot->value.string);
		fprintf(file, " } ");
	} break;
	case XS_SYMBOL_KIND: {
		fprintf(file, ".kind = XS_SYMBOL_KIND}, ");
		fprintf(file, ".value = { .symbol = %d } ", slot->value.symbol);
	} break;
	case XS_BIGINT_KIND:
	case XS_BIGINT_X_KIND: {
		fprintf(file, ".kind = XS_BIGINT_X_KIND}, ");
		fprintf(file, ".value = { .bigint = { ");
		fprintf(file, ".data = (txU4*)&gxBigIntData[%d], ", linker->bigintSize);
		fprintf(file, ".size = %d, ", slot->value.bigint.size);
		fprintf(file, ".sign = %d, ", slot->value.bigint.sign);
		fprintf(file, " } } ");
		c_memcpy(linker->bigintData + linker->bigintSize, slot->value.bigint.data, slot->value.bigint.size * sizeof(txU4));
		linker->bigintSize += slot->value.bigint.size;
	} break;
	case XS_REFERENCE_KIND: {
		fprintf(file, ".kind = XS_REFERENCE_KIND}, ");
		fprintf(file, ".value = { .reference = ");
		fxPrintAddress(the, file, slot->value.reference);
		fprintf(file, " } ");
	} break;
	case XS_CLOSURE_KIND: {
		fprintf(file, ".kind = XS_CLOSURE_KIND}, ");
		fprintf(file, ".value = { .closure = ");
		fxPrintAddress(the, file, slot->value.closure);
		fprintf(file, " } ");
	} break; 
	case XS_INSTANCE_KIND: {
		fprintf(file, ".kind = XS_INSTANCE_KIND}, ");
		fprintf(file, ".value = { .instance = { NULL, ");
		fxPrintAddress(the, file, slot->value.instance.prototype);
		fprintf(file, " } } ");
	} break;
	case XS_ARRAY_KIND: {
		fprintf(file, ".kind = XS_ARRAY_KIND}, ");
		fprintf(file, ".value = { .array = { NULL, 0 } }");
	} break;
	case XS_ARRAY_BUFFER_KIND: {
		fprintf(file, ".kind = XS_ARRAY_BUFFER_KIND}, ");
		fprintf(file, ".value = { .arrayBuffer = { NULL, 0 } }");
	} break;
	case XS_CALLBACK_KIND: {
		fprintf(file, ".kind = XS_CALLBACK_X_KIND}, ");
		fprintf(file, ".value = { .callback = { %s, NULL } }", fxGetCallbackName(the, slot->value.callback.address));
	} break;
	case XS_CODE_KIND:  {
		fprintf(file, ".kind = XS_CODE_X_KIND}, ");
		fprintf(file, ".value = { .code = { %s, ", fxGetCodeName(the, slot->value.code.address));
		fxPrintAddress(the, file, slot->value.code.closures);
		fprintf(file, " } } ");
	} break;
	case XS_CODE_X_KIND: {
		fprintf(file, ".kind = XS_CODE_X_KIND}, ");
		fprintf(file, ".value = { .code = { %s, ", fxGetCodeName(the, slot->value.code.address));
		fxPrintAddress(the, file, slot->value.code.closures);
		fprintf(file, " } } ");
	} break;
	case XS_DATE_KIND: {
		fprintf(file, ".kind = XS_DATE_KIND}, ");
		fprintf(file, ".value = { .number = ");
		fxPrintNumber(the, file, slot->value.number);
		fprintf(file, " } ");
	} break;
	case XS_DATA_VIEW_KIND: {
		fprintf(file, ".kind = XS_DATA_VIEW_KIND}, ");
		fprintf(file, ".value = { .dataView = { %d, %d } }", slot->value.dataView.offset, slot->value.dataView.size);
	} break;
	case XS_GLOBAL_KIND: {
		fprintf(file, ".kind = XS_GLOBAL_KIND}, ");
		fprintf(file, ".value = { .table = { (txSlot**)(gxGlobals), %d } }", slot->value.table.length);
	} break;
	case XS_HOST_KIND: {
		fprintf(file, ".kind = XS_HOST_KIND}, ");
		fprintf(file, ".value = { .host = { NULL, { .destructor = %s } } }", fxGetCallbackName(the, (txCallback)slot->value.host.variant.destructor ));
	} break;
	case XS_MAP_KIND: {
		fprintf(file, ".kind = XS_MAP_KIND}, ");
		fprintf(file, ".value = { .table = { NULL, %d } }", slot->value.table.length);
	} break;
	case XS_MODULE_KIND: {
		fprintf(file, ".kind = XS_MODULE_KIND}, ");
		fprintf(file, ".value = { .module = { ");
		fxPrintAddress(the, file, slot->value.module.realm);
		fprintf(file, ", %d } }", slot->value.module.id);
	} break;
	case XS_PROMISE_KIND: {
		fprintf(file, ".kind = XS_PROMISE_KIND}, ");
	} break;
	case XS_PROXY_KIND: {
		fprintf(file, ".kind = XS_PROXY_KIND}, ");
		fprintf(file, ".value = { .instance = { ");
		fxPrintAddress(the, file, slot->value.proxy.handler);
		fprintf(file, ", ");
		fxPrintAddress(the, file, slot->value.proxy.target);
		fprintf(file, " } } ");
	} break;
	case XS_REGEXP_KIND: {
		fprintf(file, ".kind = XS_REGEXP_KIND}, ");
	} break;
	case XS_SET_KIND: {
		fprintf(file, ".kind = XS_SET_KIND}, ");
		fprintf(file, ".value = { .table = { NULL, %d } }", slot->value.table.length);
	} break;
	case XS_TYPED_ARRAY_KIND: {
		fprintf(file, ".kind = XS_TYPED_ARRAY_KIND}, ");
		fprintf(file, ".value = { .typedArray = { (txTypeDispatch*)(&gxTypeDispatches[%d]), (txTypeAtomics*)(&gxTypeAtomics[%d]) } }", fxGetTypeDispatchIndex(slot->value.typedArray.dispatch), fxGetTypeAtomicsIndex(slot->value.typedArray.atomics));
	} break;
	case XS_WEAK_MAP_KIND: {
		fprintf(file, ".kind = XS_WEAK_MAP_KIND}, ");
		fprintf(file, ".value = { .table = { NULL, %d } }", slot->value.table.length);
	} break;
	case XS_WEAK_SET_KIND: {
		fprintf(file, ".kind = XS_WEAK_SET_KIND}, ");
		fprintf(file, ".value = { .table = { NULL, %d } }", slot->value.table.length);
	} break;
	case XS_ACCESSOR_KIND: {
		fprintf(file, ".kind = XS_ACCESSOR_KIND}, ");
		fprintf(file, ".value = { .accessor = { ");
		fxPrintAddress(the, file, slot->value.accessor.getter);
		fprintf(file, ", ");
		fxPrintAddress(the, file, slot->value.accessor.setter);
		fprintf(file, " } }");
	} break;
	case XS_AT_KIND: {
		fprintf(file, ".kind = XS_AT_KIND}, ");
		fprintf(file, ".value = { .at = { 0x%x, %d } }", slot->value.at.index, slot->value.at.id);
	} break;
	case XS_ENTRY_KIND: {
		fprintf(file, ".kind = XS_ENTRY_KIND}, ");
		fprintf(file, ".value = { .entry = { ");
		fxPrintAddress(the, file, slot->value.entry.slot);
		fprintf(file, ", 0x%x } }", slot->value.entry.sum);
	} break;
	case XS_ERROR_KIND: {
		fprintf(file, ".kind = XS_ERROR_KIND}, ");
		fprintf(file, ".value = { .number = 0 } ");
	} break;
	case XS_HOME_KIND: {
		fprintf(file, ".kind = XS_HOME_KIND}, ");
		fprintf(file, ".value = { .home = { ");
		fxPrintAddress(the, file, slot->value.home.object);
		fprintf(file, ", ");
		fxPrintAddress(the, file, slot->value.home.module);
		fprintf(file, " } }");
	} break;
	case XS_EXPORT_KIND: {
		fprintf(file, ".kind = XS_EXPORT_KIND}, ");
		fprintf(file, ".value = { .export = { ");
		fxPrintAddress(the, file, slot->value.export.closure);
		fprintf(file, ", ");
		fxPrintAddress(the, file, slot->value.export.module);
		fprintf(file, " } }");
	} break;
	case XS_KEY_KIND:
	case XS_KEY_X_KIND: {
		fprintf(file, ".kind = XS_KEY_X_KIND}, ");
		fprintf(file, ".value = { .key = { ");
		fxWriteCString(file, slot->value.key.string);
		fprintf(file, ", 0x%x } }", slot->value.key.sum);
	} break;
	case XS_LIST_KIND: {
		fprintf(file, ".kind = XS_LIST_KIND}, ");
		fprintf(file, ".value = { .list = { ");
		fxPrintAddress(the, file, slot->value.list.first);
		fprintf(file, ", ");
		fxPrintAddress(the, file, slot->value.list.last);
		fprintf(file, " } }");
	} break;
	case XS_STACK_KIND: {
		fprintf(file, ".kind = XS_STACK_KIND}, ");
	} break;
#ifdef mxHostFunctionPrimitive
	case XS_HOST_FUNCTION_KIND: {
		fprintf(file, ".kind = XS_HOST_FUNCTION_KIND}, ");
		fprintf(file, ".value = { .hostFunction = { %s, NULL } }", fxGetBuilderName(the, slot->value.hostFunction.builder));
	} break;
#endif
	default:
		break;
	}
	fprintf(file, "},\n");
}

void fxPrintStack(txMachine* the, FILE* file)
{
	txSlot *slot = the->stack;
	txSlot *limit = the->stackTop - 1;
	while (slot < limit) {
		fxPrintSlot(the, file, slot, XS_NO_FLAG);
		slot++;
	}
	fxPrintSlot(the, file, slot, XS_NO_FLAG);
}


void fxPrintTable(txMachine* the, FILE* file, txSize modulo, txSlot** table) 
{
	while (modulo > 1) {
		fprintf(file, "\t");
		fxPrintAddress(the, file, *table);
		fprintf(file, ",\n");
		modulo--;
		table++;
	}
	fprintf(file, "\t");
	fxPrintAddress(the, file, *table);
	fprintf(file, "\n");
}

