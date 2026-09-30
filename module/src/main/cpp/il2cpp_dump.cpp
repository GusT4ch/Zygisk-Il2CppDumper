//
// Created by Perfare on 2020/7/4.
//

#include "il2cpp_dump.h"
#include <dlfcn.h>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include "xdl.h"
#include "log.h"
#include "il2cpp-tabledefs.h"
#include "il2cpp-class.h"

#define DO_API(r, n, p) r (*n) p

#include "il2cpp-api-functions.h"

#undef DO_API

static uint64_t il2cpp_base = 0;

void init_il2cpp_api(void *handle) {
#define DO_API(r, n, p) {                      \
    n = (r (*) p)xdl_sym(handle, #n, nullptr); \
    if(!n) {                                   \
        LOGW("api not found %s", #n);          \
    }                                          \
}

#include "il2cpp-api-functions.h"

#undef DO_API
}

std::string get_method_modifier(uint32_t flags) {
    std::stringstream outPut;
    auto access = flags & METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK;
    switch (access) {
        case METHOD_ATTRIBUTE_PRIVATE:
            outPut << "private ";
            break;
        case METHOD_ATTRIBUTE_PUBLIC:
            outPut << "public ";
            break;
        case METHOD_ATTRIBUTE_FAMILY:
            outPut << "protected ";
            break;
        case METHOD_ATTRIBUTE_ASSEM:
        case METHOD_ATTRIBUTE_FAM_AND_ASSEM:
            outPut << "internal ";
            break;
        case METHOD_ATTRIBUTE_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & METHOD_ATTRIBUTE_STATIC) {
        outPut << "static ";
    }
    if (flags & METHOD_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_FINAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "sealed override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_VIRTUAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_NEW_SLOT) {
            outPut << "virtual ";
        } else {
            outPut << "override ";
        }
    }
    if (flags & METHOD_ATTRIBUTE_PINVOKE_IMPL) {
        outPut << "extern ";
    }
    return outPut.str();
}

bool _il2cpp_type_is_byref(const Il2CppType *type) {
    auto byref = type->byref;
    if (il2cpp_type_is_byref) {
        byref = il2cpp_type_is_byref(type);
    }
    return byref;
}

std::string dump_method(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Methods\n";
    void *iter = nullptr;
    while (auto method = il2cpp_class_get_methods(klass, &iter)) {
        //TODO attribute
        if (method->methodPointer) {
            outPut << "\t// RVA: 0x";
            outPut << std::hex << (uint64_t) method->methodPointer - il2cpp_base;
            outPut << " VA: 0x";
            outPut << std::hex << (uint64_t) method->methodPointer;
        } else {
            outPut << "\t// RVA: 0x VA: 0x0";
        }
        /*if (method->slot != 65535) {
            outPut << " Slot: " << std::dec << method->slot;
        }*/
        outPut << "\n\t";
        uint32_t iflags = 0;
        auto flags = il2cpp_method_get_flags(method, &iflags);
        outPut << get_method_modifier(flags);
        //TODO genericContainerIndex
        auto return_type = il2cpp_method_get_return_type(method);
        if (_il2cpp_type_is_byref(return_type)) {
            outPut << "ref ";
        }
        auto return_class = il2cpp_class_from_type(return_type);
        outPut << il2cpp_class_get_name(return_class) << " " << il2cpp_method_get_name(method)
               << "(";
        auto param_count = il2cpp_method_get_param_count(method);
        for (int i = 0; i < param_count; ++i) {
            auto param = il2cpp_method_get_param(method, i);
            auto attrs = param->attrs;
            if (_il2cpp_type_is_byref(param)) {
                if (attrs & PARAM_ATTRIBUTE_OUT && !(attrs & PARAM_ATTRIBUTE_IN)) {
                    outPut << "out ";
                } else if (attrs & PARAM_ATTRIBUTE_IN && !(attrs & PARAM_ATTRIBUTE_OUT)) {
                    outPut << "in ";
                } else {
                    outPut << "ref ";
                }
            } else {
                if (attrs & PARAM_ATTRIBUTE_IN) {
                    outPut << "[In] ";
                }
                if (attrs & PARAM_ATTRIBUTE_OUT) {
                    outPut << "[Out] ";
                }
            }
            auto parameter_class = il2cpp_class_from_type(param);
            outPut << il2cpp_class_get_name(parameter_class) << " "
                   << il2cpp_method_get_param_name(method, i);
            outPut << ", ";
        }
        if (param_count > 0) {
            outPut.seekp(-2, outPut.cur);
        }
        outPut << ") { }\n";
        //TODO GenericInstMethod
    }
    return outPut.str();
}

std::string dump_property(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Properties\n";
    void *iter = nullptr;
    while (auto prop_const = il2cpp_class_get_properties(klass, &iter)) {
        //TODO attribute
        auto prop = const_cast<PropertyInfo *>(prop_const);
        auto get = il2cpp_property_get_get_method(prop);
        auto set = il2cpp_property_get_set_method(prop);
        auto prop_name = il2cpp_property_get_name(prop);
        outPut << "\t";
        Il2CppClass *prop_class = nullptr;
        uint32_t iflags = 0;
        if (get) {
            outPut << get_method_modifier(il2cpp_method_get_flags(get, &iflags));
            prop_class = il2cpp_class_from_type(il2cpp_method_get_return_type(get));
        } else if (set) {
            outPut << get_method_modifier(il2cpp_method_get_flags(set, &iflags));
            auto param = il2cpp_method_get_param(set, 0);
            prop_class = il2cpp_class_from_type(param);
        }
        if (prop_class) {
            outPut << il2cpp_class_get_name(prop_class) << " " << prop_name << " { ";
            if (get) {
                outPut << "get; ";
            }
            if (set) {
                outPut << "set; ";
            }
            outPut << "}\n";
        } else {
            if (prop_name) {
                outPut << " // unknown property " << prop_name;
            }
        }
    }
    return outPut.str();
}

std::string dump_field(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Fields\n";
    auto is_enum = il2cpp_class_is_enum(klass);
    void *iter = nullptr;
    while (auto field = il2cpp_class_get_fields(klass, &iter)) {
        //TODO attribute
        outPut << "\t";
        auto attrs = il2cpp_field_get_flags(field);
        auto access = attrs & FIELD_ATTRIBUTE_FIELD_ACCESS_MASK;
        switch (access) {
            case FIELD_ATTRIBUTE_PRIVATE:
                outPut << "private ";
                break;
            case FIELD_ATTRIBUTE_PUBLIC:
                outPut << "public ";
                break;
            case FIELD_ATTRIBUTE_FAMILY:
                outPut << "protected ";
                break;
            case FIELD_ATTRIBUTE_ASSEMBLY:
            case FIELD_ATTRIBUTE_FAM_AND_ASSEM:
                outPut << "internal ";
                break;
            case FIELD_ATTRIBUTE_FAM_OR_ASSEM:
                outPut << "protected internal ";
                break;
        }
        if (attrs & FIELD_ATTRIBUTE_LITERAL) {
            outPut << "const ";
        } else {
            if (attrs & FIELD_ATTRIBUTE_STATIC) {
                outPut << "static ";
            }
            if (attrs & FIELD_ATTRIBUTE_INIT_ONLY) {
                outPut << "readonly ";
            }
        }
        auto field_type = il2cpp_field_get_type(field);
        auto field_class = il2cpp_class_from_type(field_type);
        outPut << il2cpp_class_get_name(field_class) << " " << il2cpp_field_get_name(field);
        //TODO 获取构造函数初始化后的字段值
        if (attrs & FIELD_ATTRIBUTE_LITERAL && is_enum) {
            uint64_t val = 0;
            il2cpp_field_static_get_value(field, &val);
            outPut << " = " << std::dec << val;
        }
        outPut << "; // 0x" << std::hex << il2cpp_field_get_offset(field) << "\n";
    }
    return outPut.str();
}

std::string dump_type(const Il2CppType *type) {
    std::stringstream outPut;
    auto *klass = il2cpp_class_from_type(type);
    outPut << "\n// Namespace: " << il2cpp_class_get_namespace(klass) << "\n";
    auto flags = il2cpp_class_get_flags(klass);
    if (flags & TYPE_ATTRIBUTE_SERIALIZABLE) {
        outPut << "[Serializable]\n";
    }
    //TODO attribute
    auto is_valuetype = il2cpp_class_is_valuetype(klass);
    auto is_enum = il2cpp_class_is_enum(klass);
    auto visibility = flags & TYPE_ATTRIBUTE_VISIBILITY_MASK;
    switch (visibility) {
        case TYPE_ATTRIBUTE_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_PUBLIC:
            outPut << "public ";
            break;
        case TYPE_ATTRIBUTE_NOT_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_FAM_AND_ASSEM:
        case TYPE_ATTRIBUTE_NESTED_ASSEMBLY:
            outPut << "internal ";
            break;
        case TYPE_ATTRIBUTE_NESTED_PRIVATE:
            outPut << "private ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAMILY:
            outPut << "protected ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & TYPE_ATTRIBUTE_ABSTRACT && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "static ";
    } else if (!(flags & TYPE_ATTRIBUTE_INTERFACE) && flags & TYPE_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
    } else if (!is_valuetype && !is_enum && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "sealed ";
    }
    if (flags & TYPE_ATTRIBUTE_INTERFACE) {
        outPut << "interface ";
    } else if (is_enum) {
        outPut << "enum ";
    } else if (is_valuetype) {
        outPut << "struct ";
    } else {
        outPut << "class ";
    }
    outPut << il2cpp_class_get_name(klass); //TODO genericContainerIndex
    std::vector<std::string> extends;
    auto parent = il2cpp_class_get_parent(klass);
    if (!is_valuetype && !is_enum && parent) {
        auto parent_type = il2cpp_class_get_type(parent);
        if (parent_type->type != IL2CPP_TYPE_OBJECT) {
            extends.emplace_back(il2cpp_class_get_name(parent));
        }
    }
    void *iter = nullptr;
    while (auto itf = il2cpp_class_get_interfaces(klass, &iter)) {
        extends.emplace_back(il2cpp_class_get_name(itf));
    }
    if (!extends.empty()) {
        outPut << " : " << extends[0];
        for (int i = 1; i < extends.size(); ++i) {
            outPut << ", " << extends[i];
        }
    }
    outPut << "\n{";
    outPut << dump_field(klass);
    outPut << dump_property(klass);
    outPut << dump_method(klass);
    //TODO EventInfo
    outPut << "}\n";
    return outPut.str();
}

void il2cpp_api_init(void *handle) {
    LOGI("il2cpp_handle: %p", handle);
    init_il2cpp_api(handle);
    if (il2cpp_domain_get_assemblies) {
        Dl_info dlInfo;
        if (dladdr((void *) il2cpp_domain_get_assemblies, &dlInfo)) {
            il2cpp_base = reinterpret_cast<uint64_t>(dlInfo.dli_fbase);
        }
        LOGI("il2cpp_base: %" PRIx64"", il2cpp_base);
    } else {
        LOGE("Failed to initialize il2cpp api.");
        return;
    }
    while (!il2cpp_is_vm_thread(nullptr)) {
        LOGI("Waiting for il2cpp_init...");
        sleep(1);
    }
    auto domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);
}

// ============================================================
// ARM64 getter instruction parser
// Scans the first N instructions of a property getter for
// LDR/LDRB/LDRH patterns that load a field from 'this' (x0).
// Tracks simple register moves (MOV Xd, X0) so it handles
// getters that save 'this' to a callee-saved register first.
// ============================================================

static Il2CppClass *find_class_all(const char *ns, const char *name) {
    size_t sz;
    auto dom = il2cpp_domain_get();
    auto asms = il2cpp_domain_get_assemblies(dom, &sz);
    for (size_t i = 0; i < sz; ++i) {
        auto img = il2cpp_assembly_get_image(asms[i]);
        auto k = il2cpp_class_from_name(img, ns, name);
        if (k) return k;
    }
    return nullptr;
}

struct ParsedField {
    uint32_t offset;
    const char *type;
};

static bool parse_arm64_getter(void *funcPtr, ParsedField *out) {
    if (!funcPtr) return false;
    auto code = reinterpret_cast<uint32_t *>(funcPtr);

    int thisReg = 0; // x0

    for (int i = 0; i < 20; ++i) {
        uint32_t insn = code[i];
        uint32_t Rn = (insn >> 5) & 0x1F;

        // MOV Xd, Xn → ORR Xd, XZR, Xn
        // 64-bit: 1010 1010 000 Rm(5) 000000 11111 Rd(5)
        if ((insn & 0xFFE0FFE0) == 0xAA0003E0) {
            uint32_t Rm = (insn >> 16) & 0x1F;
            uint32_t Rd = insn & 0x1F;
            if ((int)Rm == thisReg) thisReg = (int)Rd;
            continue;
        }

        if ((int)Rn != thisReg) {
            // RET — stop
            if ((insn & 0xFFFFFC1F) == 0xD65F0000) break;
            // Unconditional branch (B/BL) — stop
            if ((insn & 0x7C000000) == 0x14000000) break;
            continue;
        }

        // --- LDR Wt, [Xn, #imm12*4] — 32-bit int ---
        if ((insn & 0xFFC00000) == 0xB9400000) {
            out->offset = ((insn >> 10) & 0xFFF) * 4;
            out->type = "Int32";
            return true;
        }
        // --- LDR Xt, [Xn, #imm12*8] — 64-bit ptr ---
        if ((insn & 0xFFC00000) == 0xF9400000) {
            out->offset = ((insn >> 10) & 0xFFF) * 8;
            out->type = "Ptr64";
            return true;
        }
        // --- LDRB Wt, [Xn, #imm12] — byte/bool ---
        if ((insn & 0xFFC00000) == 0x39400000) {
            out->offset = (insn >> 10) & 0xFFF;
            out->type = "Bool";
            return true;
        }
        // --- LDRH Wt, [Xn, #imm12*2] — 16-bit ---
        if ((insn & 0xFFC00000) == 0x79400000) {
            out->offset = ((insn >> 10) & 0xFFF) * 2;
            out->type = "UInt16";
            return true;
        }
        // --- LDR St, [Xn, #imm12*4] — float ---
        if ((insn & 0xFFC00000) == 0xBD400000) {
            out->offset = ((insn >> 10) & 0xFFF) * 4;
            out->type = "Float";
            return true;
        }
        // --- ADD Xd, Xn, #imm — value-type address return ---
        if ((insn & 0xFF000000) == 0x91000000) {
            uint32_t imm12 = (insn >> 10) & 0xFFF;
            uint32_t sh = (insn >> 22) & 1;
            out->offset = sh ? (imm12 << 12) : imm12;
            out->type = "ValueAddr";
            return true;
        }
    }
    return false;
}

struct GetterTarget {
    const char *ns;
    const char *cls;
    const char *method;
    int params;
    const char *label;
};

// Forward declarations
static void resolve_offsets_phase2(std::string outDir);
static void resolve_offsets_phase3(std::string outDir);
static void resolve_offsets_phase4(std::string outDir);
static void resolve_offsets_phase5(std::string outDir);
static void resolve_offsets_phase6(std::string outDir);
static void resolve_offsets_phase7(std::string outDir);
static void resolve_offsets_phase8(std::string outDir);

// ============================================================
// Phase 2: Runtime value matching via il2cpp_runtime_invoke
// Spawns a background thread that waits for a live Player,
// then calls getters and matches return values against raw
// field memory to resolve obfuscated backing-field offsets.
// ============================================================

struct FieldCandidate {
    const char *name;
    uint32_t offset;
    int il2cppType;
};

static void collect_field_candidates(Il2CppClass *klass, std::vector<FieldCandidate> &out) {
    while (klass) {
        void *iter = nullptr;
        while (auto f = il2cpp_class_get_fields(klass, &iter)) {
            auto flags = il2cpp_field_get_flags(f);
            if (flags & FIELD_ATTRIBUTE_STATIC) continue;
            auto ftype = il2cpp_field_get_type(f);
            int typeEnum = il2cpp_type_get_type(ftype);
            uint32_t off = il2cpp_field_get_offset(f);
            auto fname = il2cpp_field_get_name(f);
            out.push_back({fname, (uint32_t)off, typeEnum});
        }
        klass = il2cpp_class_get_parent(klass);
        if (klass) {
            auto pname = il2cpp_class_get_name(klass);
            if (!pname || strcmp(pname, "Object") == 0 ||
                strcmp(pname, "MonoBehaviour") == 0) break;
        }
    }
}

static void resolve_offsets_phase2(std::string outDir) {
    LOGI("=== PHASE 2: Value matching thread started ===");

    // Delay de 5 segundos pra garantir IL2CPP estabilizado
    sleep(5);

    auto dom = il2cpp_domain_get();
    if (!dom) {
        LOGE("Phase2: il2cpp_domain_get() returned nullptr");
        return;
    }
    auto thr = il2cpp_thread_attach(dom);
    if (!thr) {
        LOGE("Phase2: il2cpp_thread_attach() failed");
        return;
    }
    LOGI("Phase2: thread attached to domain");

    // Find GameFacade::CurrentLocalPlayer
    auto facadeK = find_class_all("COW", "GameFacade");
    if (!facadeK) {
        LOGE("Phase2: GameFacade not found, aborting");
        return;
    }
    auto clpMethod = il2cpp_class_get_method_from_name(facadeK, "CurrentLocalPlayer", 0);
    if (!clpMethod || !clpMethod->methodPointer) {
        LOGE("Phase2: CurrentLocalPlayer method not found, aborting");
        return;
    }
    LOGI("Phase2: CurrentLocalPlayer method at %p", clpMethod->methodPointer);

    // Find target getter methods
    struct P2Getter {
        const char *ns;
        const char *cls;
        const char *method;
        const char *label;
        const MethodInfo *mi;
        int retType; // IL2CPP_TYPE_* of return
    };

    P2Getter getters[] = {
        {"COW.GamePlay", "Player",           "get_CurHP",             "CurHP",       nullptr, IL2CPP_TYPE_I4},
        {"COW.GamePlay", "Player",           "get_MaxHP",             "MaxHP",       nullptr, IL2CPP_TYPE_I4},
        {"COW.GamePlay", "Player",           "get_CurEP",            "CurEP",       nullptr, IL2CPP_TYPE_I4},
        {"COW.GamePlay", "Player",           "get_CurAP",            "CurAP",       nullptr, IL2CPP_TYPE_I4},
        {"COW.GamePlay", "Player",           "get_OriginalMaxHP",    "OrigMaxHP",   nullptr, IL2CPP_TYPE_I4},
        {"COW.GamePlay", "Player",           "get_NickName",         "NickName",    nullptr, IL2CPP_TYPE_STRING},
        {"COW.GamePlay", "Player",           "get_TeamIndex",        "TeamIndex",   nullptr, IL2CPP_TYPE_I4},
        {"COW.GamePlay", "Player",           "get_HeadBoneTransform","HeadBone",    nullptr, IL2CPP_TYPE_CLASS},
        {"COW.GamePlay", "Player",           "get_NeckBone",         "NeckBone",    nullptr, IL2CPP_TYPE_CLASS},
        {"COW.GamePlay", "Player",           "get_HipsBoneTransform","HipsBone",    nullptr, IL2CPP_TYPE_CLASS},
        {"COW.GamePlay", "AttackableEntity", "get_IsDead",           "IsDead",      nullptr, IL2CPP_TYPE_BOOLEAN},
    };

    int numGetters = sizeof(getters) / sizeof(getters[0]);
    for (int i = 0; i < numGetters; ++i) {
        auto k = find_class_all(getters[i].ns, getters[i].cls);
        if (k) {
            getters[i].mi = il2cpp_class_get_method_from_name(k, getters[i].method, 0);
            if (getters[i].mi)
                LOGI("Phase2: Found %s at %p", getters[i].label, getters[i].mi->methodPointer);
            else
                LOGW("Phase2: Method not found: %s", getters[i].method);
        }
    }

    // Collect field candidates from Player hierarchy
    auto playerK = find_class_all("COW.GamePlay", "Player");
    if (!playerK) {
        LOGE("Phase2: Player class not found, aborting");
        return;
    }
    std::vector<FieldCandidate> fields;
    collect_field_candidates(playerK, fields);
    LOGI("Phase2: Collected %zu field candidates across hierarchy", fields.size());

    uint32_t playerSize = il2cpp_class_instance_size(playerK);

    // Poll for a live Player instance
    Il2CppObject *localPlayer = nullptr;
    for (int attempt = 0; attempt < 120; ++attempt) {
        Il2CppException *exc = nullptr;
        auto result = il2cpp_runtime_invoke(clpMethod, nullptr, nullptr, &exc);
        if (exc) {
            LOGW("Phase2: CurrentLocalPlayer threw exception, attempt %d", attempt);
        } else if (result) {
            localPlayer = result;
            LOGI("Phase2: Got local player at %p (attempt %d)", localPlayer, attempt);
            break;
        }
        sleep(5);
    }

    if (!localPlayer) {
        LOGE("Phase2: Failed to get local player after 120 attempts (10 min), aborting");
        return;
    }

    // Small delay to let the player fully initialize in-match
    sleep(3);

    auto outPath = outDir + "/files/offsets_resolved.txt";
    std::ofstream out(outPath);
    if (!out.is_open()) {
        LOGE("Phase2: Cannot open %s", outPath.c_str());
        return;
    }

    out << "// =============================================\n"
        << "// Phantom Offset Resolver — Phase 2 Results\n"
        << "// Method: Runtime value matching\n"
        << "// Player instance at: 0x" << std::hex << (uint64_t)localPlayer << "\n"
        << "// =============================================\n\n";

    auto rawBase = reinterpret_cast<uint8_t *>(localPlayer);

    // For each getter, call it, get the return value, then scan fields
    for (int i = 0; i < numGetters; ++i) {
        if (!getters[i].mi || !getters[i].mi->methodPointer) {
            out << getters[i].label << " = METHOD_NOT_FOUND\n";
            continue;
        }

        Il2CppException *exc = nullptr;
        Il2CppObject *retObj = nullptr;

        // Call the getter — try up to 3 times
        for (int retry = 0; retry < 3; ++retry) {
            exc = nullptr;
            retObj = il2cpp_runtime_invoke(getters[i].mi, localPlayer, nullptr, &exc);
            if (!exc) break;
            LOGW("Phase2: %s threw on attempt %d", getters[i].label, retry);
            usleep(500000);
        }

        if (exc || !retObj) {
            out << getters[i].label << " = INVOKE_FAILED\n";
            LOGW("Phase2: %s invoke failed", getters[i].label);
            continue;
        }

        out << "[" << getters[i].label << "]\n";

        if (getters[i].retType == IL2CPP_TYPE_I4) {
            // Int32 — unbox and compare against all Int32 fields
            int32_t val = *(int32_t *)il2cpp_object_unbox(retObj);
            out << "  getter_value = " << std::dec << val
                << " (0x" << std::hex << val << ")\n";
            LOGI("Phase2: %s = %d (0x%X)", getters[i].label, val, val);

            int matches = 0;
            for (auto &fc : fields) {
                if (fc.il2cppType != IL2CPP_TYPE_I4 && fc.il2cppType != IL2CPP_TYPE_U4)
                    continue;
                if (fc.offset + 4 > playerSize) continue;
                int32_t memVal = *(int32_t *)(rawBase + fc.offset);
                if (memVal == val) {
                    out << "  MATCH " << fc.name << " @ 0x"
                        << std::hex << fc.offset
                        << " (mem=" << std::dec << memVal << ")\n";
                    LOGI("Phase2: %s MATCH %s @ 0x%X = %d",
                         getters[i].label, fc.name, fc.offset, memVal);
                    matches++;
                }
            }
            if (matches == 0)
                out << "  NO_MATCH (value not found in any Int32 field)\n";

        } else if (getters[i].retType == IL2CPP_TYPE_BOOLEAN) {
            bool val = *(bool *)il2cpp_object_unbox(retObj);
            out << "  getter_value = " << (val ? "true" : "false") << "\n";
            LOGI("Phase2: %s = %s", getters[i].label, val ? "true" : "false");

            int matches = 0;
            for (auto &fc : fields) {
                if (fc.il2cppType != IL2CPP_TYPE_BOOLEAN) continue;
                if (fc.offset + 1 > playerSize) continue;
                bool memVal = *(bool *)(rawBase + fc.offset);
                if (memVal == val) {
                    out << "  MATCH " << fc.name << " @ 0x"
                        << std::hex << fc.offset << "\n";
                    matches++;
                }
            }
            if (matches == 0)
                out << "  NO_MATCH\n";

        } else if (getters[i].retType == IL2CPP_TYPE_STRING) {
            // String — retObj IS the Il2CppString*
            auto str = reinterpret_cast<Il2CppString *>(retObj);
            int32_t len = il2cpp_string_length(str);
            auto chars = il2cpp_string_chars(str);
            // Convert UTF-16 to ASCII for logging
            std::string ascii;
            for (int c = 0; c < len && c < 64; ++c)
                ascii += (char)(chars[c] & 0x7F);
            out << "  getter_value = \"" << ascii << "\" (len=" << len << ")\n";
            LOGI("Phase2: %s = \"%s\" (len=%d)", getters[i].label, ascii.c_str(), len);

            // Match: the field stores a pointer to this same Il2CppString*
            int matches = 0;
            for (auto &fc : fields) {
                if (fc.il2cppType != IL2CPP_TYPE_STRING && fc.il2cppType != IL2CPP_TYPE_CLASS
                    && fc.il2cppType != IL2CPP_TYPE_OBJECT) continue;
                if (fc.offset + 8 > playerSize) continue;
                uint64_t memPtr = *(uint64_t *)(rawBase + fc.offset);
                if (memPtr == (uint64_t)str) {
                    out << "  MATCH " << fc.name << " @ 0x"
                        << std::hex << fc.offset << " (exact ptr)\n";
                    matches++;
                } else if (memPtr != 0) {
                    // Check if it's a different string with same content
                    auto memStr = reinterpret_cast<Il2CppString *>(memPtr);
                    // Safety: only deref if the pointer looks like a heap object
                    if (memPtr > 0x10000 && memPtr < 0x7FFFFFFFFFFF) {
                        auto memObj = reinterpret_cast<Il2CppObject *>(memPtr);
                        if (memObj->klass) {
                            auto klassName = il2cpp_class_get_name(memObj->klass);
                            if (klassName && strcmp(klassName, "String") == 0) {
                                int32_t mLen = il2cpp_string_length(memStr);
                                if (mLen == len) {
                                    auto mChars = il2cpp_string_chars(memStr);
                                    bool eq = true;
                                    for (int c = 0; c < len; ++c) {
                                        if (mChars[c] != chars[c]) { eq = false; break; }
                                    }
                                    if (eq) {
                                        out << "  MATCH " << fc.name << " @ 0x"
                                            << std::hex << fc.offset << " (same content, diff ptr)\n";
                                        matches++;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (matches == 0)
                out << "  NO_MATCH\n";

        } else if (getters[i].retType == IL2CPP_TYPE_CLASS) {
            // Object/Transform pointer — direct pointer comparison
            uint64_t ptrVal = (uint64_t)retObj;
            out << "  getter_value = 0x" << std::hex << ptrVal << "\n";
            LOGI("Phase2: %s = 0x%llX", getters[i].label,
                 (unsigned long long)ptrVal);

            int matches = 0;
            for (auto &fc : fields) {
                if (fc.il2cppType != IL2CPP_TYPE_CLASS && fc.il2cppType != IL2CPP_TYPE_OBJECT
                    && fc.il2cppType != IL2CPP_TYPE_STRING) continue;
                if (fc.offset + 8 > playerSize) continue;
                uint64_t memPtr = *(uint64_t *)(rawBase + fc.offset);
                if (memPtr == ptrVal) {
                    out << "  MATCH " << fc.name << " @ 0x"
                        << std::hex << fc.offset << "\n";
                    matches++;
                }
            }
            if (matches == 0)
                out << "  NO_MATCH\n";
        }

        out << "\n";
    }

    // === Disambiguation pass: sample Int32 values twice to separate CurHP/MaxHP ===
    out << "[Disambiguation — Second Sample (3s later)]\n";
    sleep(3);

    for (int i = 0; i < numGetters; ++i) {
        if (getters[i].retType != IL2CPP_TYPE_I4) continue;
        if (!getters[i].mi) continue;

        Il2CppException *exc = nullptr;
        auto retObj = il2cpp_runtime_invoke(getters[i].mi, localPlayer, nullptr, &exc);
        if (exc || !retObj) continue;

        int32_t val = *(int32_t *)il2cpp_object_unbox(retObj);
        out << getters[i].label << " = " << std::dec << val
            << " (0x" << std::hex << val << ")";

        // Re-scan to see which fields still match
        for (auto &fc : fields) {
            if (fc.il2cppType != IL2CPP_TYPE_I4 && fc.il2cppType != IL2CPP_TYPE_U4) continue;
            if (fc.offset + 4 > playerSize) continue;
            int32_t memVal = *(int32_t *)(rawBase + fc.offset);
            if (memVal == val) {
                out << "  [" << fc.name << "@0x" << std::hex << fc.offset << "]";
            }
        }
        out << "\n";
    }
    out << "\n";

    // === Summary: best-guess offset map ===
    out << "[Summary — Resolved Offset Map]\n";
    out << "// Format: label = 0x<offset> (<field_name>)\n";
    out << "// When multiple matches exist, all are listed.\n";
    out << "// Use disambiguation samples + game knowledge to pick.\n\n";

    for (int i = 0; i < numGetters; ++i) {
        if (!getters[i].mi) {
            out << getters[i].label << " = NOT_RESOLVED\n";
            continue;
        }

        Il2CppException *exc = nullptr;
        auto retObj = il2cpp_runtime_invoke(getters[i].mi, localPlayer, nullptr, &exc);
        if (exc || !retObj) {
            out << getters[i].label << " = INVOKE_FAILED\n";
            continue;
        }

        bool found = false;
        if (getters[i].retType == IL2CPP_TYPE_I4) {
            int32_t val = *(int32_t *)il2cpp_object_unbox(retObj);
            for (auto &fc : fields) {
                if (fc.il2cppType != IL2CPP_TYPE_I4 && fc.il2cppType != IL2CPP_TYPE_U4) continue;
                if (fc.offset + 4 > playerSize) continue;
                if (*(int32_t *)(rawBase + fc.offset) == val) {
                    out << getters[i].label << " = 0x" << std::hex << fc.offset
                        << " (" << fc.name << ") val=" << std::dec << val << "\n";
                    found = true;
                }
            }
        } else if (getters[i].retType == IL2CPP_TYPE_BOOLEAN) {
            bool val = *(bool *)il2cpp_object_unbox(retObj);
            for (auto &fc : fields) {
                if (fc.il2cppType != IL2CPP_TYPE_BOOLEAN) continue;
                if (fc.offset + 1 > playerSize) continue;
                if (*(bool *)(rawBase + fc.offset) == val) {
                    out << getters[i].label << " = 0x" << std::hex << fc.offset
                        << " (" << fc.name << ") val=" << (val ? "true" : "false") << "\n";
                    found = true;
                }
            }
        } else if (getters[i].retType == IL2CPP_TYPE_STRING) {
            auto str = reinterpret_cast<Il2CppString *>(retObj);
            for (auto &fc : fields) {
                if (fc.il2cppType != IL2CPP_TYPE_STRING && fc.il2cppType != IL2CPP_TYPE_CLASS) continue;
                if (fc.offset + 8 > playerSize) continue;
                uint64_t memPtr = *(uint64_t *)(rawBase + fc.offset);
                if (memPtr == (uint64_t)str) {
                    out << getters[i].label << " = 0x" << std::hex << fc.offset
                        << " (" << fc.name << ") exact_ptr\n";
                    found = true;
                }
            }
        } else if (getters[i].retType == IL2CPP_TYPE_CLASS) {
            uint64_t ptrVal = (uint64_t)retObj;
            for (auto &fc : fields) {
                if (fc.il2cppType != IL2CPP_TYPE_CLASS && fc.il2cppType != IL2CPP_TYPE_OBJECT) continue;
                if (fc.offset + 8 > playerSize) continue;
                if (*(uint64_t *)(rawBase + fc.offset) == ptrVal) {
                    out << getters[i].label << " = 0x" << std::hex << fc.offset
                        << " (" << fc.name << ") exact_ptr\n";
                    found = true;
                }
            }
        }

        if (!found)
            out << getters[i].label << " = NOT_RESOLVED\n";
    }

    out.close();
    LOGI("=== PHASE 2 DONE — %s ===", outPath.c_str());
}

static void resolve_offsets(const char *outDir) {
    LOGI("=== OFFSET RESOLVER START ===");

    auto outPath = std::string(outDir) + "/files/offsets.txt";
    std::ofstream out(outPath);
    if (!out.is_open()) {
        LOGE("Cannot open %s for writing", outPath.c_str());
        return;
    }

    out << "// =============================================\n"
        << "// Phantom Offset Resolver — Free Fire 1.132.1\n"
        << "// Method: ARM64 getter instruction parsing\n"
        << "// =============================================\n\n";

    // 1. Find target classes
    auto playerK    = find_class_all("COW.GamePlay", "Player");
    auto attackK    = find_class_all("COW.GamePlay", "AttackableEntity");
    auto entityK    = find_class_all("GCommon",      "Entity");
    auto facadeK    = find_class_all("COW",          "GameFacade");
    auto matchK     = find_class_all("COW",          "MatchGame");

    out << "[Classes]\n";
    out << "Player           = " << (playerK  ? "FOUND" : "MISSING") << "\n";
    out << "AttackableEntity = " << (attackK  ? "FOUND" : "MISSING") << "\n";
    out << "Entity           = " << (entityK  ? "FOUND" : "MISSING") << "\n";
    out << "GameFacade       = " << (facadeK  ? "FOUND" : "MISSING") << "\n";
    out << "MatchGame        = " << (matchK   ? "FOUND" : "MISSING") << "\n\n";

    LOGI("Classes: Player=%p Attack=%p Entity=%p Facade=%p Match=%p",
         playerK, attackK, entityK, facadeK, matchK);

    // 2. Resolve getter backing-field offsets via ARM64 parsing
    GetterTarget targets[] = {
        {"COW.GamePlay", "Player", "get_CurHP",               0, "Player::CurHP"},
        {"COW.GamePlay", "Player", "get_MaxHP",               0, "Player::MaxHP"},
        {"COW.GamePlay", "Player", "get_CurEP",               0, "Player::CurEP"},
        {"COW.GamePlay", "Player", "get_CurAP",               0, "Player::CurAP"},
        {"COW.GamePlay", "Player", "get_OriginalMaxHP",       0, "Player::OriginalMaxHP"},
        {"COW.GamePlay", "Player", "get_NickName",            0, "Player::NickName"},
        {"COW.GamePlay", "Player", "get_TeamIndex",           0, "Player::TeamIndex"},
        {"COW.GamePlay", "Player", "get_HeadBoneTransform",   0, "Player::HeadBone"},
        {"COW.GamePlay", "Player", "get_NeckBone",            0, "Player::NeckBone"},
        {"COW.GamePlay", "Player", "get_HipsBoneTransform",   0, "Player::HipsBone"},
        {"COW.GamePlay", "Player", "get_BipBoneTransform",    0, "Player::BipBone"},
        {"COW.GamePlay", "Player", "get_ShoulderBoneTransform", 0, "Player::ShoulderBone"},
        {"COW.GamePlay", "Player", "get_HandBoneLeft",        0, "Player::HandBoneLeft"},
        {"COW.GamePlay", "Player", "get_HandBoneRight",       0, "Player::HandBoneRight"},
        {"COW.GamePlay", "Player", "get_RightLegBoneTransform", 0, "Player::RightLegBone"},
        {"COW.GamePlay", "Player", "get_BoneLeftWeapon",      0, "Player::BoneLeftWeapon"},
        {"COW.GamePlay", "AttackableEntity", "get_IsDead",    0, "AttackableEntity::IsDead"},
        {"GCommon",      "Entity", "get_Position",            0, "Entity::Position"},
    };

    out << "[Getter Offsets — ARM64 Parsed]\n";

    for (auto &t : targets) {
        auto klass = find_class_all(t.ns, t.cls);
        if (!klass) {
            out << t.label << " = CLASS_NOT_FOUND\n";
            continue;
        }
        auto mi = il2cpp_class_get_method_from_name(klass, t.method, t.params);
        if (!mi || !mi->methodPointer) {
            out << t.label << " = METHOD_NOT_FOUND\n";
            continue;
        }

        uint64_t rva = (uint64_t)mi->methodPointer - il2cpp_base;
        ParsedField pf{};
        if (parse_arm64_getter(reinterpret_cast<void *>(mi->methodPointer), &pf)) {
            out << t.label << " = 0x" << std::hex << pf.offset
                << "  (" << pf.type << ")  [RVA 0x" << rva << "]\n";
            LOGI("RESOLVED  %s = 0x%X (%s)", t.label, pf.offset, pf.type);
        } else {
            // Dump raw instructions for manual analysis
            auto c = reinterpret_cast<uint32_t *>(mi->methodPointer);
            out << t.label << " = COMPLEX  [RVA 0x" << std::hex << rva << "]  insns:";
            for (int j = 0; j < 8; ++j)
                out << " " << std::hex << c[j];
            out << "\n";
            LOGW("COMPLEX  %s  RVA 0x%llX", t.label, (unsigned long long)rva);
        }
    }
    out << "\n";

    // 3. Enumerate all fields of target classes
    struct ClassEntry { Il2CppClass *k; const char *name; };
    ClassEntry classes[] = {
        {playerK,  "Player"},
        {attackK,  "AttackableEntity"},
        {entityK,  "Entity"},
        {facadeK,  "GameFacade"},
        {matchK,   "MatchGame"},
    };

    for (auto &ce : classes) {
        if (!ce.k) continue;
        out << "[Fields: " << ce.name << "]  InstanceSize=0x"
            << std::hex << il2cpp_class_instance_size(ce.k) << "\n";
        void *iter = nullptr;
        while (auto f = il2cpp_class_get_fields(ce.k, &iter)) {
            auto fname = il2cpp_field_get_name(f);
            auto ftype = il2cpp_field_get_type(f);
            auto fcls  = il2cpp_class_from_type(ftype);
            auto tname = il2cpp_class_get_name(fcls);
            auto off   = il2cpp_field_get_offset(f);
            auto flags = il2cpp_field_get_flags(f);
            bool stat  = (flags & FIELD_ATTRIBUTE_STATIC) != 0;
            out << "  " << (stat ? "static " : "")
                << tname << " " << fname
                << " // 0x" << std::hex << off << "\n";
        }
        out << "\n";
    }

    // 4. Method RVAs
    struct MethodTarget {
        const char *ns; const char *cls; const char *method;
        int params; const char *label;
    };
    MethodTarget mtargets[] = {
        {"COW",          "GameFacade", "CurrentLocalPlayer",          0, "GameFacade::CurrentLocalPlayer"},
        {"COW",          "GameFacade", "IsLocalTeammate",             1, "GameFacade::IsLocalTeammate"},
        {"COW",          "GameFacade", "CurrentLocalPlayerTeamIndex", 0, "GameFacade::CurrentLocalPlayerTeamIndex"},
        {"COW.GamePlay", "Player",     "get_CurHP",                  0, "Player::get_CurHP"},
        {"COW.GamePlay", "Player",     "get_MaxHP",                  0, "Player::get_MaxHP"},
        {"COW.GamePlay", "Player",     "get_NickName",               0, "Player::get_NickName"},
        {"COW.GamePlay", "Player",     "get_HeadBoneTransform",      0, "Player::get_HeadBoneTransform"},
        {"COW.GamePlay", "AttackableEntity", "get_IsDead",           0, "AttackableEntity::get_IsDead"},
        {"GCommon",      "Entity",     "get_Position",               0, "Entity::get_Position"},
    };

    out << "[Method RVAs]\n";
    for (auto &mt : mtargets) {
        auto klass = find_class_all(mt.ns, mt.cls);
        if (!klass) { out << mt.label << " = CLASS_NOT_FOUND\n"; continue; }
        auto mi = il2cpp_class_get_method_from_name(klass, mt.method, mt.params);
        if (!mi || !mi->methodPointer) { out << mt.label << " = NOT_FOUND\n"; continue; }
        uint64_t rva = (uint64_t)mi->methodPointer - il2cpp_base;
        out << mt.label << "  RVA=0x" << std::hex << rva
            << "  VA=0x" << (uint64_t)mi->methodPointer << "\n";
    }
    out << "\n";

    // 5. Static field data pointers
    out << "[Static Field Addresses]\n";
    if (facadeK) {
        auto sd = il2cpp_class_get_static_field_data(facadeK);
        out << "GameFacade.StaticFieldData = 0x" << std::hex << (uint64_t)sd << "\n";
        auto f1 = il2cpp_class_get_field_from_name(facadeK, "CurrentMatchGame");
        if (f1) out << "GameFacade.CurrentMatchGame  offset=0x"
                     << std::hex << il2cpp_field_get_offset(f1) << " (static)\n";
        auto f2 = il2cpp_class_get_field_from_name(facadeK, "LocalPlayerUserID");
        if (f2) out << "GameFacade.LocalPlayerUserID offset=0x"
                     << std::hex << il2cpp_field_get_offset(f2) << " (static)\n";
    }
    out << "\n";

    // 6. Non-obfuscated instance fields (direct names)
    out << "[Known Fields — Direct Name]\n";
    auto try_field = [&](Il2CppClass *k, const char *cls, const char *name) {
        if (!k) return;
        auto f = il2cpp_class_get_field_from_name(k, name);
        if (f) {
            out << cls << "::" << name << " = 0x"
                << std::hex << il2cpp_field_get_offset(f) << "\n";
        }
    };
    try_field(entityK,  "Entity",    "m_CachedTransform");
    try_field(entityK,  "Entity",    "m_UniqueID");
    try_field(matchK,   "MatchGame", "m_ReplicationEntitis");
    try_field(matchK,   "MatchGame", "m_CameraControllerManager");
    try_field(facadeK,  "GameFacade","CurrentMatchGame");
    try_field(facadeK,  "GameFacade","LocalPlayerUserID");
    out << "\n";

    // 7. Runtime info
    out << "[Runtime]\n";
    out << "il2cpp_base         = 0x" << std::hex << il2cpp_base << "\n";
    out << "object_header_size  = "   << std::dec << il2cpp_object_header_size() << "\n";
    if (playerK) {
        out << "Player.InstanceSize = 0x" << std::hex
            << il2cpp_class_instance_size(playerK) << "\n";
        out << "Player.Class        = 0x" << std::hex << (uint64_t)playerK << "\n";
    }
    if (facadeK) {
        out << "GameFacade.Class    = 0x" << std::hex << (uint64_t)facadeK << "\n";
    }
    out << "\n";

    out.close();
    LOGI("=== OFFSET RESOLVER DONE — %s ===", outPath.c_str());

    // Spawn Phase 2 + Phase 3 background threads
    std::string dirCopy(outDir);
    std::string dirCopy2(outDir);
    std::thread phase2(resolve_offsets_phase2, dirCopy);
    phase2.detach();
    std::thread phase3(resolve_offsets_phase3, dirCopy2);
    phase3.detach();
    std::string dirCopy3(outDir);
    std::thread phase4(resolve_offsets_phase4, dirCopy3);
    phase4.detach();
    std::string dirCopy4(outDir);
    std::thread phase5(resolve_offsets_phase5, dirCopy4);
    phase5.detach();
    std::string dirCopy5(outDir);
    std::thread phase6(resolve_offsets_phase6, dirCopy5);
    phase6.detach();
    std::string dirCopy6(outDir);
    std::thread phase7(resolve_offsets_phase7, dirCopy6);
    phase7.detach();
    std::string dirCopy7(outDir);
    std::thread phase8(resolve_offsets_phase8, dirCopy7);
    phase8.detach();
    LOGI("Phase 2-8 threads spawned — enter a match to resolve offsets");
}

// ============================================================
// Phase 3: Deep ARM64 disassembly of COW getters
// Dumps raw instructions + follows pointer chains to find
// where CurHP/MaxHP are actually stored (COW replication).
// Runs after Phase 2 succeeds (has Player*).
// ============================================================

static const char* arm64_reg_name(int reg) {
    static char buf[8];
    if (reg == 31) return "xzr";
    snprintf(buf, sizeof(buf), "x%d", reg);
    return buf;
}

static void dump_arm64_instructions(std::ofstream &out, void *funcPtr, const char *label, int maxInsns = 40) {
    if (!funcPtr) {
        out << "[" << label << "] FUNCTION NOT FOUND\n\n";
        return;
    }

    auto code = reinterpret_cast<uint32_t *>(funcPtr);
    out << "[" << label << "] @ " << funcPtr << "\n";
    out << "  RVA = 0x" << std::hex << ((uint64_t)funcPtr - il2cpp_base) << "\n";

    for (int i = 0; i < maxInsns; ++i) {
        uint32_t insn = code[i];
        uint64_t pc = (uint64_t)&code[i];

        out << "  " << std::hex << (pc - il2cpp_base) << ": "
            << std::hex << insn << "  ";

        uint32_t Rd = insn & 0x1F;
        uint32_t Rn = (insn >> 5) & 0x1F;
        uint32_t Rm = (insn >> 16) & 0x1F;

        // RET
        if ((insn & 0xFFFFFC1F) == 0xD65F0000) {
            out << "RET\n";
            break;
        }
        // NOP
        if (insn == 0xD503201F) {
            out << "NOP\n";
            continue;
        }
        // STP (pre/post index)
        if ((insn & 0x7FC00000) == 0x29800000 || (insn & 0x7FC00000) == 0xA9800000 ||
            (insn & 0x7FC00000) == 0x29000000 || (insn & 0x7FC00000) == 0xA9000000) {
            out << "STP ...\n";
            continue;
        }
        // LDP
        if ((insn & 0x7FC00000) == 0x29C00000 || (insn & 0x7FC00000) == 0xA9C00000 ||
            (insn & 0x7FC00000) == 0x29400000 || (insn & 0x7FC00000) == 0xA9400000) {
            out << "LDP ...\n";
            continue;
        }
        // MOV Xd, Xn (ORR Xd, XZR, Xn)
        if ((insn & 0xFFE0FFE0) == 0xAA0003E0) {
            out << "MOV " << arm64_reg_name(Rd) << ", " << arm64_reg_name(Rm) << "\n";
            continue;
        }
        // MOV Wd, Wn (ORR Wd, WZR, Wn)
        if ((insn & 0xFFE0FFE0) == 0x2A0003E0) {
            out << "MOV w" << Rd << ", w" << Rm << "\n";
            continue;
        }
        // ADRP
        if ((insn & 0x9F000000) == 0x90000000) {
            int64_t immhi = ((int64_t)(insn >> 5) & 0x7FFFF) << 2;
            int64_t immlo = (insn >> 29) & 0x3;
            int64_t imm = (immhi | immlo) << 12;
            if (imm & (1LL << 32)) imm |= ~((1LL << 33) - 1); // sign extend
            uint64_t target = (pc & ~0xFFF) + imm;
            out << "ADRP " << arm64_reg_name(Rd)
                << ", 0x" << std::hex << target << "\n";
            continue;
        }
        // LDR Xt, [Xn, #imm] — 64-bit
        if ((insn & 0xFFC00000) == 0xF9400000) {
            uint32_t imm12 = ((insn >> 10) & 0xFFF) * 8;
            out << "LDR " << arm64_reg_name(Rd)
                << ", [" << arm64_reg_name(Rn) << ", #0x" << std::hex << imm12 << "]\n";
            continue;
        }
        // LDR Wt, [Xn, #imm] — 32-bit
        if ((insn & 0xFFC00000) == 0xB9400000) {
            uint32_t imm12 = ((insn >> 10) & 0xFFF) * 4;
            out << "LDR w" << Rd
                << ", [" << arm64_reg_name(Rn) << ", #0x" << std::hex << imm12 << "]\n";
            continue;
        }
        // LDRB Wt, [Xn, #imm]
        if ((insn & 0xFFC00000) == 0x39400000) {
            uint32_t imm12 = (insn >> 10) & 0xFFF;
            out << "LDRB w" << Rd
                << ", [" << arm64_reg_name(Rn) << ", #0x" << std::hex << imm12 << "]\n";
            continue;
        }
        // LDRH Wt, [Xn, #imm]
        if ((insn & 0xFFC00000) == 0x79400000) {
            uint32_t imm12 = ((insn >> 10) & 0xFFF) * 2;
            out << "LDRH w" << Rd
                << ", [" << arm64_reg_name(Rn) << ", #0x" << std::hex << imm12 << "]\n";
            continue;
        }
        // LDR St (float), [Xn, #imm]
        if ((insn & 0xFFC00000) == 0xBD400000) {
            uint32_t imm12 = ((insn >> 10) & 0xFFF) * 4;
            out << "LDR s" << Rd
                << ", [" << arm64_reg_name(Rn) << ", #0x" << std::hex << imm12 << "]\n";
            continue;
        }
        // STR Xt, [Xn, #imm] — 64-bit
        if ((insn & 0xFFC00000) == 0xF9000000) {
            uint32_t imm12 = ((insn >> 10) & 0xFFF) * 8;
            out << "STR " << arm64_reg_name(Rd)
                << ", [" << arm64_reg_name(Rn) << ", #0x" << std::hex << imm12 << "]\n";
            continue;
        }
        // ADD Xd, Xn, #imm
        if ((insn & 0xFF000000) == 0x91000000) {
            uint32_t imm12 = (insn >> 10) & 0xFFF;
            uint32_t sh = (insn >> 22) & 1;
            uint32_t val = sh ? (imm12 << 12) : imm12;
            out << "ADD " << arm64_reg_name(Rd) << ", "
                << arm64_reg_name(Rn) << ", #0x" << std::hex << val << "\n";
            continue;
        }
        // SUB Xd, Xn, #imm
        if ((insn & 0xFF000000) == 0xD1000000) {
            uint32_t imm12 = (insn >> 10) & 0xFFF;
            out << "SUB " << arm64_reg_name(Rd) << ", "
                << arm64_reg_name(Rn) << ", #0x" << std::hex << imm12 << "\n";
            continue;
        }
        // CBZ / CBNZ
        if ((insn & 0x7E000000) == 0x34000000) {
            int32_t off19 = (int32_t)((insn >> 5) & 0x7FFFF);
            if (off19 & (1 << 18)) off19 |= ~((1 << 19) - 1);
            uint64_t target = pc + (off19 * 4);
            bool nz = (insn >> 24) & 1;
            out << (nz ? "CBNZ" : "CBZ") << " " << arm64_reg_name(Rd)
                << ", 0x" << std::hex << (target - il2cpp_base) << "\n";
            continue;
        }
        // TBZ / TBNZ
        if ((insn & 0x7E000000) == 0x36000000) {
            uint32_t bit = ((insn >> 31) << 5) | ((insn >> 19) & 0x1F);
            int32_t off14 = (int32_t)((insn >> 5) & 0x3FFF);
            if (off14 & (1 << 13)) off14 |= ~((1 << 14) - 1);
            uint64_t target = pc + (off14 * 4);
            bool nz = (insn >> 24) & 1;
            out << (nz ? "TBNZ" : "TBZ") << " " << arm64_reg_name(Rd)
                << ", #" << std::dec << bit
                << ", 0x" << std::hex << (target - il2cpp_base) << "\n";
            continue;
        }
        // B (unconditional)
        if ((insn & 0xFC000000) == 0x14000000) {
            int32_t off26 = (int32_t)(insn & 0x3FFFFFF);
            if (off26 & (1 << 25)) off26 |= ~((1 << 26) - 1);
            uint64_t target = pc + (off26 * 4);
            out << "B 0x" << std::hex << (target - il2cpp_base) << "\n";
            continue;
        }
        // BL (branch with link)
        if ((insn & 0xFC000000) == 0x94000000) {
            int32_t off26 = (int32_t)(insn & 0x3FFFFFF);
            if (off26 & (1 << 25)) off26 |= ~((1 << 26) - 1);
            uint64_t target = pc + (off26 * 4);
            out << "BL 0x" << std::hex << (target - il2cpp_base) << "\n";
            continue;
        }
        // BR (branch to register)
        if ((insn & 0xFFFFFC00) == 0xD61F0000) {
            out << "BR " << arm64_reg_name(Rn) << "\n";
            continue;
        }
        // B.cond
        if ((insn & 0xFF000010) == 0x54000000) {
            int32_t off19 = (int32_t)((insn >> 5) & 0x7FFFF);
            if (off19 & (1 << 18)) off19 |= ~((1 << 19) - 1);
            uint64_t target = pc + (off19 * 4);
            uint32_t cond = insn & 0xF;
            const char* condNames[] = {"eq","ne","cs","cc","mi","pl","vs","vc",
                                        "hi","ls","ge","lt","gt","le","al","nv"};
            out << "B." << condNames[cond]
                << " 0x" << std::hex << (target - il2cpp_base) << "\n";
            continue;
        }
        // CMP (SUBS xzr, Xn, #imm)
        if ((insn & 0xFF00001F) == 0xF100001F) {
            uint32_t imm12 = (insn >> 10) & 0xFFF;
            out << "CMP " << arm64_reg_name(Rn) << ", #0x" << std::hex << imm12 << "\n";
            continue;
        }
        // Unknown
        out << "??? (0x" << std::hex << insn << ")\n";
    }
    out << "\n";
}

static void resolve_offsets_phase3(std::string outDir) {
    LOGI("=== PHASE 3: ARM64 deep disassembly of HP getters ===");

    sleep(5);

    auto dom = il2cpp_domain_get();
    if (!dom) { LOGE("Phase3: domain null"); return; }
    auto thr = il2cpp_thread_attach(dom);
    if (!thr) { LOGE("Phase3: thread attach fail"); return; }

    auto outPath = outDir + "/files/getter_disasm.txt";
    std::ofstream out(outPath);
    if (!out.is_open()) {
        LOGE("Phase3: Cannot open %s", outPath.c_str());
        return;
    }

    out << "// =============================================\n"
        << "// Phantom Phase 3 — ARM64 Getter Disassembly\n"
        << "// il2cpp_base = 0x" << std::hex << il2cpp_base << "\n"
        << "// =============================================\n\n";

    // Getters to disassemble
    struct DisasmTarget {
        const char *ns;
        const char *cls;
        const char *method;
        const char *label;
    };

    DisasmTarget targets[] = {
        {"COW.GamePlay", "Player", "get_CurHP",          "Player::get_CurHP"},
        {"COW.GamePlay", "Player", "get_MaxHP",          "Player::get_MaxHP"},
        {"COW.GamePlay", "Player", "get_CurEP",          "Player::get_CurEP"},
        {"COW.GamePlay", "Player", "get_CurAP",          "Player::get_CurAP"},
        {"COW.GamePlay", "Player", "get_OriginalMaxHP",  "Player::get_OriginalMaxHP"},
        {"COW.GamePlay", "Player", "get_TeamIndex",      "Player::get_TeamIndex"},
        {"GCommon",      "Entity", "get_Position",       "Entity::get_Position"},
    };

    int numTargets = sizeof(targets) / sizeof(targets[0]);

    for (int i = 0; i < numTargets; ++i) {
        auto klass = find_class_all(targets[i].ns, targets[i].cls);
        if (!klass) {
            out << "[" << targets[i].label << "] CLASS NOT FOUND\n\n";
            continue;
        }

        auto mi = il2cpp_class_get_method_from_name(klass, targets[i].method, 0);
        if (!mi || !mi->methodPointer) {
            out << "[" << targets[i].label << "] METHOD NOT FOUND\n\n";
            continue;
        }

        dump_arm64_instructions(out, reinterpret_cast<void *>(mi->methodPointer), targets[i].label);

        // If getter calls a subroutine (BL), also disassemble that target
        auto code = reinterpret_cast<uint32_t *>(mi->methodPointer);
        for (int j = 0; j < 20; ++j) {
            uint32_t insn = code[j];
            // BL instruction
            if ((insn & 0xFC000000) == 0x94000000) {
                int32_t off26 = (int32_t)(insn & 0x3FFFFFF);
                if (off26 & (1 << 25)) off26 |= ~((1 << 26) - 1);
                uint64_t target = (uint64_t)&code[j] + (off26 * 4);

                char subLabel[128];
                snprintf(subLabel, sizeof(subLabel), "%s -> BL target (sub_%llX)",
                         targets[i].label, (unsigned long long)(target - il2cpp_base));

                dump_arm64_instructions(out, reinterpret_cast<void *>(target), subLabel, 30);
                break; // only follow first BL
            }
            // RET — stop looking
            if ((insn & 0xFFFFFC1F) == 0xD65F0000) break;
        }
    }

    // Also dump the runtime Player* read test
    auto facadeK = find_class_all("COW", "GameFacade");
    if (facadeK) {
        auto clpMethod = il2cpp_class_get_method_from_name(facadeK, "CurrentLocalPlayer", 0);
        if (clpMethod && clpMethod->methodPointer) {
            // Try to get Player and read from indirected offset
            Il2CppObject *localPlayer = nullptr;
            for (int attempt = 0; attempt < 60; ++attempt) {
                Il2CppException *exc = nullptr;
                auto result = il2cpp_runtime_invoke(clpMethod, nullptr, nullptr, &exc);
                if (!exc && result) {
                    localPlayer = result;
                    break;
                }
                sleep(5);
            }

            if (localPlayer) {
                out << "[Runtime Player* Test]\n";
                out << "  Player* = 0x" << std::hex << (uint64_t)localPlayer << "\n";

                auto rawBase = reinterpret_cast<uint8_t *>(localPlayer);

                // Dump first 0x300 bytes as hex for analysis
                out << "\n[Player Raw Memory Dump (first 0x300 bytes)]\n";
                for (uint32_t off = 0; off < 0x300; off += 0x10) {
                    out << "  +" << std::hex << off << ": ";
                    for (int b = 0; b < 16; ++b) {
                        uint8_t val = rawBase[off + b];
                        char hex[4];
                        snprintf(hex, sizeof(hex), "%02X ", val);
                        out << hex;
                    }
                    out << "\n";
                }

                // Dump specific pointer fields that might be COW component
                out << "\n[Pointer Fields Scan — looking for COW component]\n";
                auto playerK = find_class_all("COW.GamePlay", "Player");
                if (playerK) {
                    uint32_t pSize = il2cpp_class_instance_size(playerK);
                    void *iter = nullptr;
                    while (auto f = il2cpp_class_get_fields(playerK, &iter)) {
                        auto flags = il2cpp_field_get_flags(f);
                        if (flags & FIELD_ATTRIBUTE_STATIC) continue;
                        auto ftype = il2cpp_field_get_type(f);
                        int typeEnum = il2cpp_type_get_type(ftype);
                        // Only CLASS/OBJECT fields (pointers to other objects)
                        if (typeEnum != IL2CPP_TYPE_CLASS && typeEnum != IL2CPP_TYPE_OBJECT) continue;
                        uint32_t off = il2cpp_field_get_offset(f);
                        if (off + 8 > pSize) continue;
                        uint64_t ptr = *(uint64_t *)(rawBase + off);
                        if (ptr == 0) continue;

                        auto fname = il2cpp_field_get_name(f);
                        out << "  0x" << std::hex << off << " " << fname
                            << " = 0x" << ptr;

                        // Try to identify the class of the pointed object
                        if (ptr > 0x10000 && ptr < 0x7FFFFFFFFFFF) {
                            auto obj = reinterpret_cast<Il2CppObject *>(ptr);
                            if (obj->klass) {
                                auto kName = il2cpp_class_get_name(obj->klass);
                                auto kNs = il2cpp_class_get_namespace(obj->klass);
                                if (kName) {
                                    out << " -> " << (kNs ? kNs : "") << "::" << kName;

                                    // If it's a potential health component, try reading Int32 fields from it
                                    uint32_t subSize = il2cpp_class_instance_size(obj->klass);
                                    if (subSize > 16 && subSize < 0x200) {
                                        auto subBase = reinterpret_cast<uint8_t *>(ptr);
                                        out << " (size=0x" << std::hex << subSize << ")";
                                        // Scan for value 200 (default MaxHP) in this sub-object
                                        for (uint32_t sOff = 0x10; sOff + 4 <= subSize; sOff += 4) {
                                            int32_t sVal = *(int32_t *)(subBase + sOff);
                                            if (sVal >= 50 && sVal <= 300) {
                                                out << " [+" << std::hex << sOff << "=" << std::dec << sVal << "]";
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        out << "\n";
                    }
                }
            } else {
                out << "[Runtime Player* Test] FAILED — no player found (not in match)\n";
            }
        }
    }

    out.close();
    LOGI("=== PHASE 3 DONE — %s ===", outPath.c_str());
}

// ============================================================
// Phase 4: COW HP Deep Scan
// Follows pointer chains to find where CurHP/MaxHP are stored
// in COW sub-objects, and iterates remote players for correlation.
// ============================================================

static void dump_subobject_int32_fields(std::ofstream &out, Il2CppObject *obj,
                                         const char *prefix, int32_t curHP, int32_t maxHP) {
    if (!obj || !obj->klass) return;
    uint32_t sz = il2cpp_class_instance_size(obj->klass);
    auto base = reinterpret_cast<uint8_t *>(obj);

    void *it = nullptr;
    while (auto f = il2cpp_class_get_fields(obj->klass, &it)) {
        auto fl = il2cpp_field_get_flags(f);
        if (fl & FIELD_ATTRIBUTE_STATIC) continue;
        auto ft = il2cpp_field_get_type(f);
        int te = il2cpp_type_get_type(ft);
        auto fn = il2cpp_field_get_name(f);
        uint32_t fo = il2cpp_field_get_offset(f);

        auto fcls = il2cpp_class_from_type(ft);
        auto tn = fcls ? il2cpp_class_get_name(fcls) : "?";

        out << prefix << "0x" << std::hex << fo << " " << tn << " " << fn;

        if (te == IL2CPP_TYPE_I4 && fo + 4 <= sz) {
            int32_t v = *(int32_t *)(base + fo);
            out << " = " << std::dec << v;
            if (v == curHP) out << " *** CurHP ***";
            if (v == maxHP) out << " *** MaxHP ***";
        } else if (te == IL2CPP_TYPE_R4 && fo + 4 <= sz) {
            float v = *(float *)(base + fo);
            out << " = " << v << "f";
        } else if (te == IL2CPP_TYPE_BOOLEAN && fo + 1 <= sz) {
            out << " = " << (*(bool *)(base + fo) ? "true" : "false");
        } else if (te == IL2CPP_TYPE_I2 && fo + 2 <= sz) {
            out << " = " << std::dec << *(int16_t *)(base + fo);
        } else if ((te == IL2CPP_TYPE_CLASS || te == IL2CPP_TYPE_OBJECT) && fo + 8 <= sz) {
            uint64_t p = *(uint64_t *)(base + fo);
            out << " = 0x" << std::hex << p;
            if (p > 0x10000 && p < 0x7FFFFFFFFFFF) {
                auto o = reinterpret_cast<Il2CppObject *>(p);
                if (o->klass) {
                    auto n = il2cpp_class_get_name(o->klass);
                    auto ns = il2cpp_class_get_namespace(o->klass);
                    out << " -> " << (ns && ns[0] ? ns : "") << "::" << (n ? n : "?");
                }
            }
        }
        out << "\n";
    }
}

static void resolve_offsets_phase4(std::string outDir) {
    LOGI("=== PHASE 4: COW HP deep scan ===");
    sleep(10);

    auto dom = il2cpp_domain_get();
    if (!dom) { LOGE("Phase4: domain null"); return; }
    auto thr = il2cpp_thread_attach(dom);
    if (!thr) { LOGE("Phase4: thread attach fail"); return; }

    auto outPath = outDir + "/files/hp_deep_scan.txt";
    std::ofstream out(outPath);
    if (!out.is_open()) {
        LOGE("Phase4: Cannot open %s", outPath.c_str());
        return;
    }

    auto facadeK = find_class_all("COW", "GameFacade");
    auto playerK = find_class_all("COW.GamePlay", "Player");
    auto matchK  = find_class_all("COW", "MatchGame");
    if (!facadeK || !playerK) {
        out << "Phase4: Missing classes\n";
        out.close(); return;
    }

    auto clpMethod = il2cpp_class_get_method_from_name(facadeK, "CurrentLocalPlayer", 0);
    auto getCurHP  = il2cpp_class_get_method_from_name(playerK, "get_CurHP", 0);
    auto getMaxHP  = il2cpp_class_get_method_from_name(playerK, "get_MaxHP", 0);
    if (!clpMethod || !getCurHP || !getMaxHP || !getCurHP->methodPointer) {
        out << "Phase4: Missing methods\n";
        out.close(); return;
    }

    Il2CppObject *localPlayer = nullptr;
    int32_t curHP = 0, maxHP = 0;

    for (int attempt = 0; attempt < 180; ++attempt) {
        Il2CppException *exc = nullptr;
        auto r = il2cpp_runtime_invoke(clpMethod, nullptr, nullptr, &exc);
        if (!exc && r) {
            localPlayer = r;
            // Check if player is alive (HP > 0)
            exc = nullptr;
            auto rHP = il2cpp_runtime_invoke(getCurHP, localPlayer, nullptr, &exc);
            if (!exc && rHP) curHP = *(int32_t *)il2cpp_object_unbox(rHP);
            exc = nullptr;
            auto rMax = il2cpp_runtime_invoke(getMaxHP, localPlayer, nullptr, &exc);
            if (!exc && rMax) maxHP = *(int32_t *)il2cpp_object_unbox(rMax);

            if (curHP > 0 && maxHP > 0) break;
            LOGI("Phase4: waiting for alive player (CurHP=%d MaxHP=%d, attempt %d)", curHP, maxHP, attempt);
        }
        sleep(3);
    }
    if (!localPlayer || curHP <= 0 || maxHP <= 0) {
        out << "Phase4: Player not alive after 9min wait (CurHP=" << curHP << " MaxHP=" << maxHP << ")\n";
        out.close(); return;
    }

    out << "// =============================================\n"
        << "// Phantom Phase 4 — COW HP Deep Scan\n"
        << "// =============================================\n\n"
        << "Player* = 0x" << std::hex << (uint64_t)localPlayer << "\n"
        << "CurHP = " << std::dec << curHP << "  (0x" << std::hex << curHP << ")\n"
        << "MaxHP = " << std::dec << maxHP << "  (0x" << std::hex << maxHP << ")\n\n";

    auto rawBase = reinterpret_cast<uint8_t *>(localPlayer);
    uint32_t pSize = il2cpp_class_instance_size(playerK);

    // === 1. Scan ALL pointer sub-objects for HP values (1 + 2 levels deep) ===
    out << "[Deep Pointer Scan — CurHP=" << std::dec << curHP
        << " MaxHP=" << maxHP << "]\n";

    void *iter = nullptr;
    while (auto f = il2cpp_class_get_fields(playerK, &iter)) {
        auto flags = il2cpp_field_get_flags(f);
        if (flags & FIELD_ATTRIBUTE_STATIC) continue;
        auto ftype = il2cpp_field_get_type(f);
        int typeEnum = il2cpp_type_get_type(ftype);
        if (typeEnum != IL2CPP_TYPE_CLASS && typeEnum != IL2CPP_TYPE_OBJECT) continue;

        uint32_t off = il2cpp_field_get_offset(f);
        if (off + 8 > pSize) continue;
        uint64_t ptr = *(uint64_t *)(rawBase + off);
        if (ptr == 0 || ptr < 0x10000 || ptr > 0x7FFFFFFFFFFF) continue;

        auto obj = reinterpret_cast<Il2CppObject *>(ptr);
        if (!obj->klass) continue;

        uint32_t subSize = il2cpp_class_instance_size(obj->klass);
        if (subSize < 16) continue;

        auto subBase = reinterpret_cast<uint8_t *>(ptr);
        auto fname = il2cpp_field_get_name(f);
        auto kName = il2cpp_class_get_name(obj->klass);
        auto kNs   = il2cpp_class_get_namespace(obj->klass);

        // Level 1: scan this sub-object for HP values
        bool foundCur = false, foundMax = false;
        for (uint32_t s = 0x10; s + 4 <= subSize; s += 4) {
            int32_t v = *(int32_t *)(subBase + s);
            if (v == curHP && curHP != 0) foundCur = true;
            if (v == maxHP && maxHP != 0) foundMax = true;
        }

        if (foundCur || foundMax) {
            out << "  L1 HIT: +0x" << std::hex << off << " " << fname
                << " -> " << (kNs && kNs[0] ? kNs : "") << "::" << (kName ? kName : "?")
                << " (size=0x" << subSize << ")\n";
            for (uint32_t s = 0x10; s + 4 <= subSize; s += 4) {
                int32_t v = *(int32_t *)(subBase + s);
                if (v == curHP && curHP != 0)
                    out << "    [+0x" << std::hex << s << "] = " << std::dec << v << " *** CurHP ***\n";
                if (v == maxHP && maxHP != 0)
                    out << "    [+0x" << std::hex << s << "] = " << std::dec << v << " *** MaxHP ***\n";
            }
        }

        // Level 2: follow nested pointers
        void *subIter = nullptr;
        while (auto sf = il2cpp_class_get_fields(obj->klass, &subIter)) {
            auto sfl = il2cpp_field_get_flags(sf);
            if (sfl & FIELD_ATTRIBUTE_STATIC) continue;
            auto sft = il2cpp_field_get_type(sf);
            int ste = il2cpp_type_get_type(sft);
            if (ste != IL2CPP_TYPE_CLASS && ste != IL2CPP_TYPE_OBJECT) continue;

            uint32_t soff = il2cpp_field_get_offset(sf);
            if (soff + 8 > subSize) continue;
            uint64_t sptr = *(uint64_t *)(subBase + soff);
            if (sptr == 0 || sptr < 0x10000 || sptr > 0x7FFFFFFFFFFF) continue;

            auto sobj = reinterpret_cast<Il2CppObject *>(sptr);
            if (!sobj->klass) continue;

            uint32_t ssSize = il2cpp_class_instance_size(sobj->klass);
            if (ssSize < 16 || ssSize > 0x1000) continue;
            auto ssBase = reinterpret_cast<uint8_t *>(sptr);

            for (uint32_t ss = 0x10; ss + 4 <= ssSize; ss += 4) {
                int32_t v = *(int32_t *)(ssBase + ss);
                if ((v == curHP && curHP != 0) || (v == maxHP && maxHP != 0)) {
                    auto sfn = il2cpp_field_get_name(sf);
                    auto skN = il2cpp_class_get_name(sobj->klass);
                    auto skNs = il2cpp_class_get_namespace(sobj->klass);
                    out << "  L2 HIT: +0x" << std::hex << off << "." << sfn
                        << " -> " << (skNs && skNs[0] ? skNs : "") << "::" << (skN ? skN : "?")
                        << " [+0x" << ss << "] = " << std::dec << v;
                    if (v == curHP) out << " *** CurHP ***";
                    if (v == maxHP) out << " *** MaxHP ***";
                    out << "\n";
                }
            }
        }
    }

    // === 2. Deep dump of PlayerAttributes (Player+0x768) ===
    out << "\n[PlayerAttributes Full Dump (Player+0x768)]\n";
    {
        uint64_t paPtr = *(uint64_t *)(rawBase + 0x768);
        if (paPtr && paPtr > 0x10000 && paPtr < 0x7FFFFFFFFFFF) {
            auto paObj = reinterpret_cast<Il2CppObject *>(paPtr);
            if (paObj->klass) {
                auto n = il2cpp_class_get_name(paObj->klass);
                auto ns = il2cpp_class_get_namespace(paObj->klass);
                uint32_t sz = il2cpp_class_instance_size(paObj->klass);
                out << "  Class: " << (ns && ns[0] ? ns : "") << "::" << (n ? n : "?")
                    << "  Size: 0x" << std::hex << sz << "\n  Fields:\n";
                dump_subobject_int32_fields(out, paObj, "    ", curHP, maxHP);

                // Raw hex dump
                auto paBase = reinterpret_cast<uint8_t *>(paPtr);
                uint32_t dumpLen = sz < 0x600 ? sz : 0x600;
                out << "  Raw (" << std::dec << dumpLen << " bytes):\n";
                for (uint32_t d = 0; d < dumpLen; d += 0x10) {
                    out << "    +" << std::hex << d << ":";
                    for (uint32_t b = 0; b < 16 && d + b < dumpLen; ++b) {
                        char hex[4];
                        snprintf(hex, sizeof(hex), " %02X", paBase[d + b]);
                        out << hex;
                    }
                    // Also show Int32 interpretation
                    out << "  |";
                    for (uint32_t b = 0; b + 4 <= 16 && d + b + 4 <= dumpLen; b += 4) {
                        int32_t iv = *(int32_t *)(paBase + d + b);
                        out << " " << std::dec << iv;
                    }
                    out << "\n";
                }
            }
        } else {
            out << "  NULL\n";
        }
    }

    // === 3. Deep dump of GMOCOOEIFMK (Player+0x740) ===
    out << "\n[GMOCOOEIFMK Full Dump (Player+0x740)]\n";
    {
        uint64_t gPtr = *(uint64_t *)(rawBase + 0x740);
        if (gPtr && gPtr > 0x10000 && gPtr < 0x7FFFFFFFFFFF) {
            auto gObj = reinterpret_cast<Il2CppObject *>(gPtr);
            if (gObj->klass) {
                auto n = il2cpp_class_get_name(gObj->klass);
                auto ns = il2cpp_class_get_namespace(gObj->klass);
                uint32_t sz = il2cpp_class_instance_size(gObj->klass);
                out << "  Class: " << (ns && ns[0] ? ns : "") << "::" << (n ? n : "?")
                    << "  Size: 0x" << std::hex << sz << "\n  Fields:\n";
                dump_subobject_int32_fields(out, gObj, "    ", curHP, maxHP);
            }
        } else {
            out << "  NULL\n";
        }
    }

    // === 4. Disassemble COW value accessor (tail call target from get_CurHP) ===
    out << "\n[COW Value Accessor Disassembly]\n";
    {
        auto code = reinterpret_cast<uint32_t *>(getCurHP->methodPointer);
        for (int j = 0; j < 40; ++j) {
            uint32_t insn = code[j];
            if ((insn & 0xFC000000) == 0x14000000) {
                int32_t off26 = (int32_t)(insn & 0x3FFFFFF);
                if (off26 & (1 << 25)) off26 |= ~((1 << 26) - 1);
                uint64_t target = (uint64_t)&code[j] + ((int64_t)off26 * 4);
                uint64_t targetRVA = target - il2cpp_base;
                out << "  B target RVA = 0x" << std::hex << targetRVA << "\n";
                dump_arm64_instructions(out, reinterpret_cast<void *>(target),
                                       "COW_ValueAccessor", 80);
                break;
            }
            if ((insn & 0xFFFFFC1F) == 0xD65F0000) break;
        }
    }

    // === 5. Remote players: iterate entity list ===
    out << "\n[Remote Players Scan]\n";
    if (matchK) {
        auto cmgField = il2cpp_class_get_field_from_name(facadeK, "CurrentMatchGame");
        if (cmgField) {
            auto sd = il2cpp_class_get_static_field_data(facadeK);
            if (sd) {
                uint32_t cmgOff = il2cpp_field_get_offset(cmgField);
                uint64_t matchPtr = *(uint64_t *)((uint8_t *)sd + cmgOff);

                if (matchPtr > 0x10000) {
                    out << "  MatchGame* = 0x" << std::hex << matchPtr << "\n";
                    uint64_t listPtr = *(uint64_t *)((uint8_t *)matchPtr + 0xC0);

                    if (listPtr > 0x10000) {
                        uint64_t arrayPtr = *(uint64_t *)((uint8_t *)listPtr + 0x10);
                        int32_t count = *(int32_t *)((uint8_t *)listPtr + 0x18);
                        out << "  Entity count = " << std::dec << count << "\n";

                        int remoteScanned = 0;
                        if (arrayPtr > 0x10000 && count > 0 && count < 200) {
                            for (int e = 0; e < count && remoteScanned < 5; ++e) {
                                uint64_t ePtr = *(uint64_t *)((uint8_t *)arrayPtr + 0x20 + e * 8);
                                if (!ePtr || ePtr < 0x10000) continue;
                                if (ePtr == (uint64_t)localPlayer) continue;

                                auto eObj = reinterpret_cast<Il2CppObject *>(ePtr);
                                if (!eObj->klass) continue;
                                auto eName = il2cpp_class_get_name(eObj->klass);
                                if (!eName || strcmp(eName, "Player") != 0) continue;

                                auto rBase = reinterpret_cast<uint8_t *>(ePtr);
                                uint64_t ptr70 = *(uint64_t *)(rBase + 0x70);

                                int32_t rCurHP = 0, rMaxHP = 0;
                                {
                                    Il2CppException *exc = nullptr;
                                    auto r = il2cpp_runtime_invoke(getCurHP, eObj, nullptr, &exc);
                                    if (!exc && r) rCurHP = *(int32_t *)il2cpp_object_unbox(r);
                                }
                                {
                                    Il2CppException *exc = nullptr;
                                    auto r = il2cpp_runtime_invoke(getMaxHP, eObj, nullptr, &exc);
                                    if (!exc && r) rMaxHP = *(int32_t *)il2cpp_object_unbox(r);
                                }

                                out << "\n  Remote #" << std::dec << e
                                    << " @ 0x" << std::hex << ePtr
                                    << "  CurHP=" << std::dec << rCurHP
                                    << "  MaxHP=" << rMaxHP
                                    << "  +0x70=0x" << std::hex << ptr70 << "\n";

                                // Dump Player+0x70 sub-object if non-null
                                if (ptr70 > 0x10000 && ptr70 < 0x7FFFFFFFFFFF) {
                                    auto p70Obj = reinterpret_cast<Il2CppObject *>(ptr70);
                                    if (p70Obj->klass) {
                                        auto p70Name = il2cpp_class_get_name(p70Obj->klass);
                                        auto p70Ns = il2cpp_class_get_namespace(p70Obj->klass);
                                        uint32_t p70Size = il2cpp_class_instance_size(p70Obj->klass);
                                        out << "    +0x70 -> " << (p70Ns && p70Ns[0] ? p70Ns : "")
                                            << "::" << (p70Name ? p70Name : "?")
                                            << " (size=0x" << std::hex << p70Size << ")\n";
                                        dump_subobject_int32_fields(out, p70Obj, "      ", rCurHP, rMaxHP);
                                    }
                                }

                                // Dump remote PlayerAttributes (0x768)
                                uint64_t rpaPtr = *(uint64_t *)(rBase + 0x768);
                                if (rpaPtr > 0x10000 && rpaPtr < 0x7FFFFFFFFFFF) {
                                    auto rpaObj = reinterpret_cast<Il2CppObject *>(rpaPtr);
                                    if (rpaObj->klass) {
                                        out << "    PlayerAttributes (+0x768):\n";
                                        dump_subobject_int32_fields(out, rpaObj, "      ", rCurHP, rMaxHP);
                                    }
                                }

                                // Dump remote GMOCOOEIFMK (0x740)
                                uint64_t rgPtr = *(uint64_t *)(rBase + 0x740);
                                if (rgPtr > 0x10000 && rgPtr < 0x7FFFFFFFFFFF) {
                                    auto rgObj = reinterpret_cast<Il2CppObject *>(rgPtr);
                                    if (rgObj->klass) {
                                        out << "    GMOCOOEIFMK (+0x740):\n";
                                        dump_subobject_int32_fields(out, rgObj, "      ", rCurHP, rMaxHP);
                                    }
                                }

                                ++remoteScanned;
                            }
                        }
                        if (remoteScanned == 0)
                            out << "  No remote Players found in entity list\n";
                    }
                }
            }
        }
    }

    out.close();
    LOGI("=== PHASE 4 DONE — %s ===", outPath.c_str());
}

// ============================================================
// Phase 5: COW component tracing + entity list fix
// Traces the actual COW component object returned by interface
// dispatch, dumps its data buffer, and fixes entity iteration.
// ============================================================

static void resolve_offsets_phase5(std::string outDir) {
    LOGI("=== PHASE 5: COW component trace ===");
    sleep(15);

    auto dom = il2cpp_domain_get();
    if (!dom) { LOGE("Phase5: domain null"); return; }
    auto thr = il2cpp_thread_attach(dom);
    if (!thr) { LOGE("Phase5: thread attach fail"); return; }

    auto outPath = outDir + "/files/cow_trace.txt";
    std::ofstream out(outPath);
    if (!out.is_open()) { LOGE("Phase5: Cannot open %s", outPath.c_str()); return; }

    auto facadeK = find_class_all("COW", "GameFacade");
    auto playerK = find_class_all("COW.GamePlay", "Player");
    auto matchK  = find_class_all("COW", "MatchGame");
    if (!facadeK || !playerK) { out << "Missing classes\n"; out.close(); return; }

    auto clpMethod = il2cpp_class_get_method_from_name(facadeK, "CurrentLocalPlayer", 0);
    auto getCurHP  = il2cpp_class_get_method_from_name(playerK, "get_CurHP", 0);
    auto getMaxHP  = il2cpp_class_get_method_from_name(playerK, "get_MaxHP", 0);
    if (!clpMethod || !getCurHP || !getMaxHP) { out << "Missing methods\n"; out.close(); return; }

    // Wait for alive player
    Il2CppObject *localPlayer = nullptr;
    int32_t curHP = 0, maxHP = 0;
    for (int attempt = 0; attempt < 180; ++attempt) {
        Il2CppException *exc = nullptr;
        auto r = il2cpp_runtime_invoke(clpMethod, nullptr, nullptr, &exc);
        if (!exc && r) {
            localPlayer = r;
            exc = nullptr;
            auto rH = il2cpp_runtime_invoke(getCurHP, localPlayer, nullptr, &exc);
            if (!exc && rH) curHP = *(int32_t *)il2cpp_object_unbox(rH);
            exc = nullptr;
            auto rM = il2cpp_runtime_invoke(getMaxHP, localPlayer, nullptr, &exc);
            if (!exc && rM) maxHP = *(int32_t *)il2cpp_object_unbox(rM);
            if (curHP > 0 && maxHP > 0) break;
        }
        sleep(3);
    }
    if (!localPlayer || curHP <= 0) {
        out << "Phase5: Player not alive\n"; out.close(); return;
    }

    out << "// =============================================\n"
        << "// Phantom Phase 5 — COW Component Trace\n"
        << "// =============================================\n\n"
        << "Player* = 0x" << std::hex << (uint64_t)localPlayer << "\n"
        << "CurHP = " << std::dec << curHP << "  MaxHP = " << maxHP << "\n\n";

    // === 1. Enumerate MatchGame fields to find entity collection ===
    out << "[MatchGame Fields]\n";
    if (matchK) {
        uint32_t mSize = il2cpp_class_instance_size(matchK);
        out << "  InstanceSize = 0x" << std::hex << mSize << "\n";

        auto cmgField = il2cpp_class_get_field_from_name(facadeK, "CurrentMatchGame");
        Il2CppObject *matchObj = nullptr;
        if (cmgField) {
            auto sd = il2cpp_class_get_static_field_data(facadeK);
            if (sd) {
                uint32_t cmgOff = il2cpp_field_get_offset(cmgField);
                uint64_t mPtr = *(uint64_t *)((uint8_t *)sd + cmgOff);
                if (mPtr > 0x10000) matchObj = reinterpret_cast<Il2CppObject *>(mPtr);
            }
        }

        if (matchObj) {
            out << "  MatchGame* = 0x" << std::hex << (uint64_t)matchObj << "\n";
            auto mBase = reinterpret_cast<uint8_t *>(matchObj);

            void *iter = nullptr;
            while (auto f = il2cpp_class_get_fields(matchK, &iter)) {
                auto flags = il2cpp_field_get_flags(f);
                if (flags & FIELD_ATTRIBUTE_STATIC) continue;
                auto ftype = il2cpp_field_get_type(f);
                int typeEnum = il2cpp_type_get_type(ftype);
                auto fname = il2cpp_field_get_name(f);
                uint32_t foff = il2cpp_field_get_offset(f);
                auto fcls = il2cpp_class_from_type(ftype);
                auto tname = fcls ? il2cpp_class_get_name(fcls) : "?";

                // Only show fields that might be entity containers
                if (typeEnum == IL2CPP_TYPE_CLASS || typeEnum == IL2CPP_TYPE_OBJECT ||
                    typeEnum == IL2CPP_TYPE_GENERICINST) {
                    uint64_t ptr = 0;
                    if (foff + 8 <= mSize)
                        ptr = *(uint64_t *)(mBase + foff);

                    out << "  0x" << std::hex << foff << " " << tname << " " << fname
                        << " = 0x" << ptr;

                    if (ptr > 0x10000 && ptr < 0x7FFFFFFFFFFF) {
                        auto obj = reinterpret_cast<Il2CppObject *>(ptr);
                        if (obj->klass) {
                            auto kn = il2cpp_class_get_name(obj->klass);
                            auto kns = il2cpp_class_get_namespace(obj->klass);
                            out << " -> " << (kns && kns[0] ? kns : "") << "::" << (kn ? kn : "?");

                            // If it's a List, try to read count
                            if (kn && strstr(kn, "List")) {
                                auto lBase = reinterpret_cast<uint8_t *>(ptr);
                                int32_t cnt = *(int32_t *)(lBase + 0x18);
                                uint64_t arr = *(uint64_t *)(lBase + 0x10);
                                out << " [count=" << std::dec << cnt
                                    << " items=0x" << std::hex << arr << "]";
                            }
                        }
                    }
                    out << "\n";
                }
            }

            // Also try iterating using parent class fields (Entity inherits)
            out << "\n  [MatchGame parent class fields]\n";
            auto parentK = il2cpp_class_get_parent(matchK);
            while (parentK) {
                auto pname = il2cpp_class_get_name(parentK);
                if (!pname || strcmp(pname, "Object") == 0 || strcmp(pname, "MonoBehaviour") == 0) break;
                out << "  Parent: " << pname << "\n";
                void *piter = nullptr;
                while (auto pf = il2cpp_class_get_fields(parentK, &piter)) {
                    auto pflags = il2cpp_field_get_flags(pf);
                    if (pflags & FIELD_ATTRIBUTE_STATIC) continue;
                    auto pftype = il2cpp_field_get_type(pf);
                    int ptypeEnum = il2cpp_type_get_type(pftype);
                    if (ptypeEnum != IL2CPP_TYPE_CLASS && ptypeEnum != IL2CPP_TYPE_OBJECT &&
                        ptypeEnum != IL2CPP_TYPE_GENERICINST) continue;
                    auto pfname = il2cpp_field_get_name(pf);
                    uint32_t pfoff = il2cpp_field_get_offset(pf);
                    auto pfcls = il2cpp_class_from_type(pftype);
                    auto ptname = pfcls ? il2cpp_class_get_name(pfcls) : "?";

                    uint64_t ptr = 0;
                    if (pfoff + 8 <= mSize)
                        ptr = *(uint64_t *)(mBase + pfoff);

                    out << "    0x" << std::hex << pfoff << " " << ptname << " " << pfname
                        << " = 0x" << ptr;

                    if (ptr > 0x10000 && ptr < 0x7FFFFFFFFFFF) {
                        auto obj = reinterpret_cast<Il2CppObject *>(ptr);
                        if (obj->klass) {
                            auto kn = il2cpp_class_get_name(obj->klass);
                            out << " -> " << (kn ? kn : "?");
                            if (kn && strstr(kn, "List")) {
                                auto lBase = reinterpret_cast<uint8_t *>(ptr);
                                int32_t cnt = *(int32_t *)(lBase + 0x18);
                                out << " [count=" << std::dec << cnt << "]";
                            }
                        }
                    }
                    out << "\n";
                }
                parentK = il2cpp_class_get_parent(parentK);
            }
        }
    }

    // === 2. Raw scan Player memory for HP value (including pointers to buffers) ===
    out << "\n[Raw Memory Scan for CurHP=" << std::dec << curHP << " (0x"
        << std::hex << curHP << ")]\n";
    {
        auto rawBase = reinterpret_cast<uint8_t *>(localPlayer);
        uint32_t pSize = il2cpp_class_instance_size(playerK);

        // Scan all pointer fields, follow them, and scan the pointed buffer
        // for the HP value as both Int32 AND as part of larger structures
        void *iter = nullptr;
        while (auto f = il2cpp_class_get_fields(playerK, &iter)) {
            auto flags = il2cpp_field_get_flags(f);
            if (flags & FIELD_ATTRIBUTE_STATIC) continue;
            auto ftype = il2cpp_field_get_type(f);
            int typeEnum = il2cpp_type_get_type(ftype);
            if (typeEnum != IL2CPP_TYPE_CLASS && typeEnum != IL2CPP_TYPE_OBJECT) continue;

            uint32_t off = il2cpp_field_get_offset(f);
            if (off + 8 > pSize) continue;
            uint64_t ptr = *(uint64_t *)(rawBase + off);
            if (ptr == 0 || ptr < 0x10000 || ptr > 0x7FFFFFFFFFFF) continue;

            auto obj = reinterpret_cast<Il2CppObject *>(ptr);
            if (!obj->klass) continue;

            uint32_t subSize = il2cpp_class_instance_size(obj->klass);
            if (subSize < 16) continue;

            auto subBase = reinterpret_cast<uint8_t *>(ptr);

            // Scan raw bytes for curHP value (4-byte aligned)
            for (uint32_t s = 0x10; s + 4 <= subSize; s += 4) {
                int32_t v = *(int32_t *)(subBase + s);
                if (v == curHP) {
                    auto fname = il2cpp_field_get_name(f);
                    auto kn = il2cpp_class_get_name(obj->klass);
                    out << "  +0x" << std::hex << off << " (" << fname << " -> " << kn
                        << ") raw[+0x" << s << "] = " << std::dec << v << " *** CurHP ***\n";
                }
                if (v == maxHP && maxHP != curHP) {
                    auto fname = il2cpp_field_get_name(f);
                    auto kn = il2cpp_class_get_name(obj->klass);
                    out << "  +0x" << std::hex << off << " (" << fname << " -> " << kn
                        << ") raw[+0x" << s << "] = " << std::dec << v << " *** MaxHP ***\n";
                }
            }

            // Also follow nested pointers (Class/Object fields in sub-objects)
            // and scan their data buffers
            void *subIter = nullptr;
            while (auto sf = il2cpp_class_get_fields(obj->klass, &subIter)) {
                auto sfl = il2cpp_field_get_flags(sf);
                if (sfl & FIELD_ATTRIBUTE_STATIC) continue;
                auto sft = il2cpp_field_get_type(sf);
                int ste = il2cpp_type_get_type(sft);
                if (ste != IL2CPP_TYPE_CLASS && ste != IL2CPP_TYPE_OBJECT) continue;

                uint32_t soff = il2cpp_field_get_offset(sf);
                if (soff + 8 > subSize) continue;
                uint64_t sptr = *(uint64_t *)(subBase + soff);
                if (sptr == 0 || sptr < 0x10000 || sptr > 0x7FFFFFFFFFFF) continue;

                auto sobj = reinterpret_cast<Il2CppObject *>(sptr);
                if (!sobj->klass) continue;

                uint32_t ssSize = il2cpp_class_instance_size(sobj->klass);
                if (ssSize < 16 || ssSize > 0x2000) continue;
                auto ssBase = reinterpret_cast<uint8_t *>(sptr);

                for (uint32_t ss = 0x10; ss + 4 <= ssSize; ss += 4) {
                    int32_t v = *(int32_t *)(ssBase + ss);
                    if (v == curHP) {
                        auto fname = il2cpp_field_get_name(f);
                        auto sfname = il2cpp_field_get_name(sf);
                        auto skn = il2cpp_class_get_name(sobj->klass);
                        out << "  +0x" << std::hex << off << "." << sfname
                            << " (" << fname << " -> " << skn
                            << ") raw[+0x" << ss << "] = " << std::dec << v
                            << " *** CurHP ***\n";
                    }
                }
            }

            // Also check: if this object has Array-like fields (SZARRAY),
            // scan the array contents for HP value
            void *arrIter = nullptr;
            while (auto af = il2cpp_class_get_fields(obj->klass, &arrIter)) {
                auto afl = il2cpp_field_get_flags(af);
                if (afl & FIELD_ATTRIBUTE_STATIC) continue;
                auto aft = il2cpp_field_get_type(af);
                int ate = il2cpp_type_get_type(aft);
                if (ate != IL2CPP_TYPE_SZARRAY) continue;

                uint32_t aoff = il2cpp_field_get_offset(af);
                if (aoff + 8 > subSize) continue;
                uint64_t aptr = *(uint64_t *)(subBase + aoff);
                if (aptr == 0 || aptr < 0x10000 || aptr > 0x7FFFFFFFFFFF) continue;

                // IL2CPP array: klass(8), monitor(8), bounds(8), max_length(8), vector[]
                auto arr = reinterpret_cast<Il2CppArray *>(aptr);
                uint64_t arrLen = arr->max_length;
                if (arrLen == 0 || arrLen > 1000) continue;

                auto vecBase = reinterpret_cast<uint8_t *>(arr) + 0x20;
                for (uint64_t ai = 0; ai < arrLen && ai < 200; ++ai) {
                    int32_t v = *(int32_t *)(vecBase + ai * 4);
                    if (v == curHP) {
                        auto fname = il2cpp_field_get_name(f);
                        auto afname = il2cpp_field_get_name(af);
                        out << "  +0x" << std::hex << off << "." << afname << "["
                            << std::dec << ai << "] = " << v << " *** CurHP ***\n";
                    }
                }
            }
        }
    }

    // === 3. Direct getter function call + trace COW component ===
    out << "\n[COW Component Trace]\n";
    {
        // The get_CurHP function uses type token 0x23D4
        // At RVA 0x882aa1c: checks interface availability
        // At RVA 0x882aad0: gets the COW component object
        // We call the getter directly and then look at Player+0x70 and nearby
        // to find the component object

        // Try to find the COW HP component by calling getter AND looking at
        // what the getter returns vs the component hierarchy
        auto rawBase = reinterpret_cast<uint8_t *>(localPlayer);

        // Check Player+0x70 (fallback path in getter)
        uint64_t ptr70 = *(uint64_t *)(rawBase + 0x70);
        out << "  Player+0x70 = 0x" << std::hex << ptr70;
        if (ptr70 > 0x10000 && ptr70 < 0x7FFFFFFFFFFF) {
            auto obj70 = reinterpret_cast<Il2CppObject *>(ptr70);
            if (obj70->klass) {
                auto n = il2cpp_class_get_name(obj70->klass);
                auto ns = il2cpp_class_get_namespace(obj70->klass);
                uint32_t sz = il2cpp_class_instance_size(obj70->klass);
                out << " -> " << (ns && ns[0] ? ns : "") << "::" << (n ? n : "?")
                    << " (size=0x" << std::hex << sz << ")\n";
                dump_subobject_int32_fields(out, obj70, "    ", curHP, maxHP);
            } else {
                out << " (no klass)\n";
            }
        } else {
            out << " (NULL)\n";
        }

        // Try to find the getter's COW component via type token search
        // Scan all classes for type token 0x23D4 and 0x23D5
        out << "\n  [Type Token Search — 0x23D4 (CurHP) and 0x23D5 (MaxHP)]\n";
        size_t numAssm = 0;
        auto assemblies = il2cpp_domain_get_assemblies(dom, &numAssm);
        for (size_t i = 0; i < numAssm; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            if (!image) continue;
            auto classCount = il2cpp_image_get_class_count(image);
            for (size_t j = 0; j < classCount; ++j) {
                auto klass = const_cast<Il2CppClass *>(il2cpp_image_get_class(image, j));
                if (!klass) continue;
                uint32_t token = il2cpp_class_get_type_token(klass);
                if (token == 0x23D4 || token == 0x23D5 ||
                    token == 0x020023D4 || token == 0x020023D5) {
                    auto tn = il2cpp_class_get_name(klass);
                    auto tns = il2cpp_class_get_namespace(klass);
                    uint32_t sz = il2cpp_class_instance_size(klass);
                    out << "    Token 0x" << std::hex << token << " -> "
                        << (tns && tns[0] ? tns : "") << "::" << (tn ? tn : "?")
                        << " (size=0x" << sz << ")\n";

                    // Enumerate fields of this type
                    out << "    Fields:\n";
                    void *titer = nullptr;
                    while (auto tf = il2cpp_class_get_fields(klass, &titer)) {
                        auto tfl = il2cpp_field_get_flags(tf);
                        if (tfl & FIELD_ATTRIBUTE_STATIC) continue;
                        auto tft = il2cpp_field_get_type(tf);
                        auto tfcls = il2cpp_class_from_type(tft);
                        auto tfn = il2cpp_field_get_name(tf);
                        auto ttn = tfcls ? il2cpp_class_get_name(tfcls) : "?";
                        uint32_t tfo = il2cpp_field_get_offset(tf);
                        out << "      0x" << std::hex << tfo << " "
                            << ttn << " " << tfn << "\n";
                    }
                }
            }
        }
    }

    out.close();
    LOGI("=== PHASE 5 DONE — %s ===", outPath.c_str());
}

// ============================================================
// Phase 6: PRIDataPool deep trace + Dictionary entity iteration
// Dumps raw bytes of PRIDataPool, follows every pointer to find
// the COW replication data buffer where HP is stored.
// Also iterates the Dictionary<int,Entity> for remote players.
// ============================================================

static bool safe_readable(void *addr, size_t len) {
    if (!addr || (uint64_t)addr < 0x10000 || (uint64_t)addr > 0x7FFFFFFFFFFF) return false;
    int fd = open("/dev/null", O_WRONLY);
    if (fd < 0) return false;
    bool ok = (write(fd, addr, len) == (ssize_t)len);
    close(fd);
    return ok;
}

static void resolve_offsets_phase6(std::string outDir) {
    LOGI("=== PHASE 6: PRIDataPool deep trace ===");
    sleep(25);

    auto dom = il2cpp_domain_get();
    if (!dom) { LOGE("Phase6: domain null"); return; }
    auto thr = il2cpp_thread_attach(dom);
    if (!thr) { LOGE("Phase6: thread attach fail"); return; }

    auto outPath = outDir + "/files/pridatapool_trace.txt";
    std::ofstream out(outPath);
    if (!out.is_open()) { LOGE("Phase6: Cannot open %s", outPath.c_str()); return; }

    auto facadeK = find_class_all("COW", "GameFacade");
    auto playerK = find_class_all("COW.GamePlay", "Player");
    if (!facadeK || !playerK) { out << "Missing classes\n"; out.close(); return; }

    auto clpMethod = il2cpp_class_get_method_from_name(facadeK, "CurrentLocalPlayer", 0);
    auto getCurHP  = il2cpp_class_get_method_from_name(playerK, "get_CurHP", 0);
    auto getMaxHP  = il2cpp_class_get_method_from_name(playerK, "get_MaxHP", 0);
    if (!clpMethod || !getCurHP || !getMaxHP) { out << "Missing methods\n"; out.close(); return; }

    Il2CppObject *localPlayer = nullptr;
    int32_t curHP = 0, maxHP = 0;
    for (int attempt = 0; attempt < 180; ++attempt) {
        Il2CppException *exc = nullptr;
        auto r = il2cpp_runtime_invoke(clpMethod, nullptr, nullptr, &exc);
        if (!exc && r) {
            localPlayer = r;
            exc = nullptr;
            auto rH = il2cpp_runtime_invoke(getCurHP, localPlayer, nullptr, &exc);
            if (!exc && rH) curHP = *(int32_t *)il2cpp_object_unbox(rH);
            exc = nullptr;
            auto rM = il2cpp_runtime_invoke(getMaxHP, localPlayer, nullptr, &exc);
            if (!exc && rM) maxHP = *(int32_t *)il2cpp_object_unbox(rM);
            if (curHP > 0 && maxHP > 0) break;
        }
        sleep(3);
    }
    if (!localPlayer || curHP <= 0) {
        out << "Phase6: Player not alive\n"; out.close(); return;
    }

    out << "// =============================================\n"
        << "// Phantom Phase 6 — PRIDataPool Deep Trace\n"
        << "// =============================================\n\n"
        << "Player* = 0x" << std::hex << (uint64_t)localPlayer << "\n"
        << "CurHP = " << std::dec << curHP << "  MaxHP = " << maxHP << "\n\n";

    auto rawBase = reinterpret_cast<uint8_t *>(localPlayer);

    // === 1. Dump PRIDataPool (Player+0x70) raw bytes ===
    out << "[PRIDataPool Raw Dump — Player+0x70]\n";
    uint64_t priPtr = *(uint64_t *)(rawBase + 0x70);
    out << "  PRIDataPool* = 0x" << std::hex << priPtr << "\n";
    if (safe_readable((void *)priPtr, 0x40)) {
        auto priBase = reinterpret_cast<uint8_t *>(priPtr);
        auto priObj = reinterpret_cast<Il2CppObject *>(priPtr);
        uint32_t priSize = 0x40;
        if (priObj->klass) {
            priSize = il2cpp_class_instance_size(priObj->klass);
            auto pn = il2cpp_class_get_name(priObj->klass);
            out << "  Class: " << (pn ? pn : "?") << " size=0x" << std::hex << priSize << "\n";

            out << "  [All fields incl. inherited]\n";
            auto k = priObj->klass;
            while (k) {
                auto kname = il2cpp_class_get_name(k);
                if (!kname || strcmp(kname, "Object") == 0) break;
                void *iter = nullptr;
                while (auto f = il2cpp_class_get_fields(k, &iter)) {
                    auto fflags = il2cpp_field_get_flags(f);
                    if (fflags & FIELD_ATTRIBUTE_STATIC) continue;
                    auto fname = il2cpp_field_get_name(f);
                    uint32_t foff = il2cpp_field_get_offset(f);
                    auto ftype = il2cpp_field_get_type(f);
                    auto fcls = il2cpp_class_from_type(ftype);
                    auto tname = fcls ? il2cpp_class_get_name(fcls) : "?";
                    out << "    0x" << std::hex << foff << " " << tname << " " << fname
                        << " (" << kname << ")\n";
                }
                k = il2cpp_class_get_parent(k);
            }
        }

        // Raw hex dump
        out << "\n  [Raw Hex — 0x" << std::hex << priSize << " bytes]\n  ";
        for (uint32_t i = 0; i < priSize && i < 0x100; ++i) {
            char hex[4];
            snprintf(hex, sizeof(hex), "%02x", priBase[i]);
            out << hex;
            if ((i + 1) % 8 == 0) out << " ";
            if ((i + 1) % 32 == 0) out << "\n  ";
        }
        out << "\n";

        // Follow every 8-byte aligned pointer in PRIDataPool
        out << "\n  [Following pointers in PRIDataPool]\n";
        for (uint32_t off = 0x10; off + 8 <= priSize; off += 8) {
            uint64_t ptr = *(uint64_t *)(priBase + off);
            if (!safe_readable((void *)ptr, 0x10)) continue;

            out << "  +0x" << std::hex << off << " = 0x" << ptr;

            auto pObj = reinterpret_cast<Il2CppObject *>(ptr);
            if (pObj->klass) {
                auto kn = il2cpp_class_get_name(pObj->klass);
                auto ns = il2cpp_class_get_namespace(pObj->klass);
                if (kn) {
                    uint32_t sz = il2cpp_class_instance_size(pObj->klass);
                    out << " -> " << (ns && ns[0] ? ns : "") << "::" << kn
                        << " (size=0x" << sz << ")";

                    if (safe_readable((void *)ptr, sz)) {
                        auto subBase = reinterpret_cast<uint8_t *>(ptr);
                        for (uint32_t s = 0x10; s + 4 <= sz && s < 0x400; s += 4) {
                            int32_t v = *(int32_t *)(subBase + s);
                            if (v == curHP) {
                                out << "\n    *** raw[+0x" << s << "] = " << std::dec << v
                                    << " *** MATCH CurHP ***" << std::hex;
                            }
                        }

                        if (sz <= 0x80) {
                            out << "\n    Hex: ";
                            for (uint32_t h = 0; h < sz; ++h) {
                                char hx[4];
                                snprintf(hx, sizeof(hx), "%02x", subBase[h]);
                                out << hx;
                                if ((h + 1) % 8 == 0) out << " ";
                            }
                        }
                    }
                }
            }
            out << "\n";
        }
    }

    // === 2. Scan raw Player memory for the HP value as float ===
    out << "\n[Float Scan — looking for " << std::dec << curHP << ".0f in Player]\n";
    {
        float fCurHP = (float)curHP;
        uint32_t pSize = il2cpp_class_instance_size(playerK);
        for (uint32_t off = 0x10; off + 4 <= pSize; off += 4) {
            float v = *(float *)(rawBase + off);
            if (v == fCurHP) {
                out << "  Player+0x" << std::hex << off << " = " << std::dec << v
                    << "f *** MATCH CurHP as float ***\n";
            }
        }
    }

    // === 3. Dictionary iteration for remote players ===
    out << "\n[Dictionary Entity Iteration]\n";
    {
        auto matchK = find_class_all("COW", "MatchGame");
        auto attackableK = find_class_all("COW.GamePlay", "AttackableEntity");
        if (matchK) {
            auto cmgField = il2cpp_class_get_field_from_name(facadeK, "CurrentMatchGame");
            Il2CppObject *matchObj = nullptr;
            if (cmgField) {
                auto sd = il2cpp_class_get_static_field_data(facadeK);
                if (sd) {
                    uint32_t cmgOff = il2cpp_field_get_offset(cmgField);
                    uint64_t mPtr = *(uint64_t *)((uint8_t *)sd + cmgOff);
                    if (mPtr > 0x10000) matchObj = reinterpret_cast<Il2CppObject *>(mPtr);
                }
            }

            if (matchObj) {
                auto mBase = reinterpret_cast<uint8_t *>(matchObj);
                uint64_t dictPtr = *(uint64_t *)(mBase + 0xC0);
                out << "  m_ReplicationEntitis (Dict) = 0x" << std::hex << dictPtr << "\n";

                if (safe_readable((void *)dictPtr, 0x40)) {
                    auto dictBase = reinterpret_cast<uint8_t *>(dictPtr);

                    // Dump raw dict header to discover layout
                    out << "  [Dict raw header 0x40 bytes]\n  ";
                    for (int i = 0; i < 0x40; ++i) {
                        char hx[4];
                        snprintf(hx, sizeof(hx), "%02x", dictBase[i]);
                        out << hx;
                        if ((i + 1) % 8 == 0) out << " ";
                        if ((i + 1) % 32 == 0) out << "\n  ";
                    }
                    out << "\n";

                    int32_t count   = *(int32_t *)(dictBase + 0x20);
                    uint64_t entriesPtr = *(uint64_t *)(dictBase + 0x18);
                    out << "  count=" << std::dec << count
                        << " entries=0x" << std::hex << entriesPtr << "\n";

                    if (count > 0 && count < 200 && safe_readable((void *)entriesPtr, 0x20)) {
                        auto entArr = reinterpret_cast<Il2CppArray *>(entriesPtr);
                        uint64_t arrLen = entArr->max_length;
                        out << "  entries max_length=" << std::dec << arrLen << "\n";

                        // Dump first few entries raw to detect stride
                        auto vecBase = reinterpret_cast<uint8_t *>(entArr) + 0x20;
                        out << "  [First 3 entries raw (96 bytes)]\n  ";
                        uint32_t dumpSz = 96;
                        if (safe_readable(vecBase, dumpSz)) {
                            for (uint32_t i = 0; i < dumpSz; ++i) {
                                char hx[4];
                                snprintf(hx, sizeof(hx), "%02x", vecBase[i]);
                                out << hx;
                                if ((i + 1) % 8 == 0) out << " ";
                                if ((i + 1) % 32 == 0) out << "\n  ";
                            }
                        }
                        out << "\n";

                        // Try stride=24 iteration, only on valid Player/AttackableEntity
                        int found = 0;
                        for (uint64_t e = 0; e < arrLen && e < 100 && found < 15; ++e) {
                            uint8_t *ent = vecBase + e * 24;
                            if (!safe_readable(ent, 24)) break;

                            int32_t hash = *(int32_t *)(ent + 0);
                            int32_t key  = *(int32_t *)(ent + 8);
                            uint64_t val = *(uint64_t *)(ent + 16);

                            if (hash == -1) continue;
                            if (!safe_readable((void *)val, 0x10)) continue;

                            auto entObj = reinterpret_cast<Il2CppObject *>(val);
                            if (!entObj->klass) continue;
                            auto en = il2cpp_class_get_name(entObj->klass);

                            out << "\n  [" << std::dec << e << "] key=" << key
                                << " -> " << (en ? en : "?")
                                << " @ 0x" << std::hex << val;

                            bool isLocal = (val == (uint64_t)localPlayer);
                            out << (isLocal ? " (LOCAL)" : "");

                            // Only call getters on Player instances
                            bool isPlayer = il2cpp_class_is_subclass_of(entObj->klass, playerK, true);
                            out << (isPlayer ? " [Player]" : " [NotPlayer]");

                            if (!isLocal && isPlayer) {
                                Il2CppException *exc = nullptr;
                                auto rHP = il2cpp_runtime_invoke(getCurHP, entObj, nullptr, &exc);
                                int32_t rCur = 0;
                                if (!exc && rHP) rCur = *(int32_t *)il2cpp_object_unbox(rHP);
                                else out << " (getCurHP exc)";

                                exc = nullptr;
                                auto rMHP = il2cpp_runtime_invoke(getMaxHP, entObj, nullptr, &exc);
                                int32_t rMax = 0;
                                if (!exc && rMHP) rMax = *(int32_t *)il2cpp_object_unbox(rMHP);

                                out << " CurHP=" << std::dec << rCur << " MaxHP=" << rMax;

                                // Check Player+0x70 on remote
                                auto rBase = reinterpret_cast<uint8_t *>(val);
                                uint64_t rPri = *(uint64_t *)(rBase + 0x70);
                                out << " +0x70=0x" << std::hex << rPri;
                                if (safe_readable((void *)rPri, 0x40)) {
                                    auto rpObj = reinterpret_cast<Il2CppObject *>(rPri);
                                    if (rpObj->klass) {
                                        auto rpn = il2cpp_class_get_name(rpObj->klass);
                                        uint32_t rpSz = il2cpp_class_instance_size(rpObj->klass);
                                        out << " -> " << (rpn ? rpn : "?")
                                            << " (size=0x" << rpSz << ")";
                                    }
                                }
                            }
                            out << "\n";
                            ++found;
                        }
                        if (found == 0) out << "  No valid entities found\n";
                    }
                }
            }
        }
    }

    out.close();
    LOGI("=== PHASE 6 DONE — %s ===", outPath.c_str());
}

// ============================================================
// Phase 7: ReplicationData[] deep dive
// Expands m_Datas array from PRIDataPool to find where CurHP/MaxHP
// are stored within the COW replication data buffer.
// ============================================================

static void resolve_offsets_phase7(std::string outDir) {
    LOGI("=== PHASE 7: ReplicationData deep dive ===");
    sleep(30);

    auto dom = il2cpp_domain_get();
    if (!dom) { LOGE("Phase7: domain null"); return; }
    auto thr = il2cpp_thread_attach(dom);
    if (!thr) { LOGE("Phase7: thread attach fail"); return; }

    auto outPath = outDir + "/files/replication_data.txt";
    std::ofstream out(outPath);
    if (!out.is_open()) { LOGE("Phase7: Cannot open %s", outPath.c_str()); return; }

    auto facadeK = find_class_all("COW", "GameFacade");
    auto playerK = find_class_all("COW.GamePlay", "Player");
    if (!facadeK || !playerK) { out << "Missing classes\n"; out.close(); return; }

    auto clpMethod = il2cpp_class_get_method_from_name(facadeK, "CurrentLocalPlayer", 0);
    auto getCurHP  = il2cpp_class_get_method_from_name(playerK, "get_CurHP", 0);
    auto getMaxHP  = il2cpp_class_get_method_from_name(playerK, "get_MaxHP", 0);
    if (!clpMethod || !getCurHP || !getMaxHP) { out << "Missing methods\n"; out.close(); return; }

    Il2CppObject *localPlayer = nullptr;
    int32_t curHP = 0, maxHP = 0;
    for (int attempt = 0; attempt < 180; ++attempt) {
        Il2CppException *exc = nullptr;
        auto r = il2cpp_runtime_invoke(clpMethod, nullptr, nullptr, &exc);
        if (!exc && r) {
            localPlayer = r;
            exc = nullptr;
            auto rH = il2cpp_runtime_invoke(getCurHP, localPlayer, nullptr, &exc);
            if (!exc && rH) curHP = *(int32_t *)il2cpp_object_unbox(rH);
            exc = nullptr;
            auto rM = il2cpp_runtime_invoke(getMaxHP, localPlayer, nullptr, &exc);
            if (!exc && rM) maxHP = *(int32_t *)il2cpp_object_unbox(rM);
            if (curHP > 0 && maxHP > 0) break;
        }
        sleep(3);
    }
    if (!localPlayer || curHP <= 0) {
        out << "Phase7: Player not alive\n"; out.close(); return;
    }

    out << "// =============================================\n"
        << "// Phantom Phase 7 — ReplicationData Deep Dive\n"
        << "// =============================================\n\n"
        << "Player* = 0x" << std::hex << (uint64_t)localPlayer << "\n"
        << "CurHP = " << std::dec << curHP << "  MaxHP = " << maxHP << "\n\n";

    auto rawBase = reinterpret_cast<uint8_t *>(localPlayer);

    // Get PRIDataPool at Player+0x70
    uint64_t priPtr = *(uint64_t *)(rawBase + 0x70);
    if (!safe_readable((void *)priPtr, 0x40)) {
        out << "PRIDataPool not readable\n"; out.close(); return;
    }
    auto priBase = reinterpret_cast<uint8_t *>(priPtr);
    out << "PRIDataPool* = 0x" << std::hex << priPtr << "\n";

    // m_Datas is at PRIDataPool+0x10 (inherited from ReplicationDataPool)
    uint64_t datasPtr = *(uint64_t *)(priBase + 0x10);
    int32_t maxVarCount = *(int32_t *)(priBase + 0x18);
    uint64_t handlersPtr = *(uint64_t *)(priBase + 0x20);
    out << "m_Datas (ReplicationData[]) = 0x" << std::hex << datasPtr << "\n"
        << "m_MaxVarCount = " << std::dec << maxVarCount << "\n"
        << "m_Handlers (Object[]) = 0x" << std::hex << handlersPtr << "\n\n";

    // === 1. Expand m_Datas array ===
    out << "[ReplicationData[] m_Datas]\n";
    if (safe_readable((void *)datasPtr, 0x20)) {
        auto datasArr = reinterpret_cast<Il2CppArray *>(datasPtr);
        uint64_t arrLen = datasArr->max_length;
        out << "  Array length = " << std::dec << arrLen << "\n";

        // Check element type
        auto arrObj = reinterpret_cast<Il2CppObject *>(datasPtr);
        if (arrObj->klass) {
            auto elemK = il2cpp_class_get_element_class(arrObj->klass);
            if (elemK) {
                auto en = il2cpp_class_get_name(elemK);
                auto ens = il2cpp_class_get_namespace(elemK);
                uint32_t esz = il2cpp_class_instance_size(elemK);
                out << "  Element type: " << (ens && ens[0] ? ens : "") << "::" << (en ? en : "?")
                    << " (instance_size=0x" << std::hex << esz << ")\n";

                // Dump element type's fields
                out << "  [ReplicationData fields]\n";
                auto fk = elemK;
                while (fk) {
                    auto kname = il2cpp_class_get_name(fk);
                    if (!kname || strcmp(kname, "Object") == 0) break;
                    void *iter = nullptr;
                    while (auto f = il2cpp_class_get_fields(fk, &iter)) {
                        auto fflags = il2cpp_field_get_flags(f);
                        if (fflags & FIELD_ATTRIBUTE_STATIC) continue;
                        auto fname = il2cpp_field_get_name(f);
                        uint32_t foff = il2cpp_field_get_offset(f);
                        auto ftype = il2cpp_field_get_type(f);
                        auto fcls = il2cpp_class_from_type(ftype);
                        auto tname = fcls ? il2cpp_class_get_name(fcls) : "?";
                        out << "    0x" << std::hex << foff << " " << tname << " " << fname
                            << " (" << kname << ")\n";
                    }
                    fk = il2cpp_class_get_parent(fk);
                }

                // Check if it's a value type (struct) or reference type
                bool isValueType = il2cpp_class_is_valuetype(elemK);
                out << "  isValueType = " << (isValueType ? "true" : "false") << "\n\n";

                // Determine element stride
                uint32_t elemStride;
                if (isValueType) {
                    // For value types in arrays, stride = instance_size - header
                    elemStride = esz - 0x10;
                    if (elemStride == 0) elemStride = 8;
                } else {
                    elemStride = 8; // pointer
                }
                out << "  Computed stride = " << std::dec << elemStride << " bytes\n\n";

                // Iterate elements
                auto arrData = reinterpret_cast<uint8_t *>(datasArr) + 0x20; // skip Il2CppArray header
                uint64_t scanCount = arrLen < 32 ? arrLen : 32;

                for (uint64_t i = 0; i < scanCount; ++i) {
                    out << "  [" << std::dec << i << "] ";

                    if (isValueType) {
                        // Inline struct in array
                        uint8_t *elemBase = arrData + i * elemStride;
                        if (!safe_readable(elemBase, elemStride)) {
                            out << "(not readable)\n";
                            continue;
                        }
                        // Dump raw hex
                        out << "raw: ";
                        for (uint32_t b = 0; b < elemStride && b < 128; ++b) {
                            char hx[4];
                            snprintf(hx, sizeof(hx), "%02x", elemBase[b]);
                            out << hx;
                            if ((b + 1) % 8 == 0) out << " ";
                        }
                        out << "\n";
                        // Scan for HP value
                        for (uint32_t s = 0; s + 4 <= elemStride; s += 4) {
                            int32_t v = *(int32_t *)(elemBase + s);
                            if (v == curHP) {
                                out << "    *** MATCH CurHP=" << std::dec << v
                                    << " at element[" << i << "]+0x" << std::hex << s << " ***\n";
                            }
                            if (v == maxHP && maxHP != curHP) {
                                out << "    *** MATCH MaxHP=" << std::dec << v
                                    << " at element[" << i << "]+0x" << std::hex << s << " ***\n";
                            }
                            float fv = *(float *)(elemBase + s);
                            if (fv == (float)curHP) {
                                out << "    *** MATCH CurHP as float=" << std::dec << fv
                                    << " at element[" << i << "]+0x" << std::hex << s << " ***\n";
                            }
                        }
                        // Follow any pointers in the struct
                        for (uint32_t p = 0; p + 8 <= elemStride; p += 8) {
                            uint64_t ptr = *(uint64_t *)(elemBase + p);
                            if (safe_readable((void *)ptr, 0x10)) {
                                auto pObj = reinterpret_cast<Il2CppObject *>(ptr);
                                if (pObj->klass) {
                                    auto pn = il2cpp_class_get_name(pObj->klass);
                                    auto pns = il2cpp_class_get_namespace(pObj->klass);
                                    out << "    +0x" << std::hex << p << " -> "
                                        << (pns && pns[0] ? pns : "") << "::" << (pn ? pn : "?") << "\n";
                                }
                            }
                        }
                    } else {
                        // Reference type: each element is a pointer
                        uint64_t elemPtr = *(uint64_t *)(arrData + i * 8);
                        if (!elemPtr || !safe_readable((void *)elemPtr, 0x10)) {
                            out << "null\n";
                            continue;
                        }
                        auto elemObj = reinterpret_cast<Il2CppObject *>(elemPtr);
                        auto kn = il2cpp_class_get_name(elemObj->klass);
                        auto kns = il2cpp_class_get_namespace(elemObj->klass);
                        uint32_t ksz = il2cpp_class_instance_size(elemObj->klass);
                        out << (kns && kns[0] ? kns : "") << "::" << (kn ? kn : "?")
                            << " @ 0x" << std::hex << elemPtr
                            << " (size=0x" << ksz << ")\n";

                        // Dump raw bytes
                        if (safe_readable((void *)elemPtr, ksz)) {
                            auto eBase = reinterpret_cast<uint8_t *>(elemPtr);
                            out << "    hex: ";
                            uint32_t dSz = ksz < 256 ? ksz : 256;
                            for (uint32_t b = 0; b < dSz; ++b) {
                                char hx[4];
                                snprintf(hx, sizeof(hx), "%02x", eBase[b]);
                                out << hx;
                                if ((b + 1) % 8 == 0) out << " ";
                                if ((b + 1) % 64 == 0) out << "\n         ";
                            }
                            out << "\n";

                            // Scan for HP
                            for (uint32_t s = 0x10; s + 4 <= ksz; s += 4) {
                                int32_t v = *(int32_t *)(eBase + s);
                                if (v == curHP) {
                                    out << "    *** MATCH CurHP=" << std::dec << v
                                        << " at +0x" << std::hex << s << " ***\n";
                                }
                                if (v == maxHP && maxHP != curHP) {
                                    out << "    *** MATCH MaxHP=" << std::dec << v
                                        << " at +0x" << std::hex << s << " ***\n";
                                }
                                float fv = *(float *)(eBase + s);
                                if (fv == (float)curHP) {
                                    out << "    *** MATCH CurHP as float=" << std::dec << fv
                                        << " at +0x" << std::hex << s << " ***\n";
                                }
                            }

                            // Dump fields of this element
                            out << "    [fields]\n";
                            auto fk2 = elemObj->klass;
                            while (fk2) {
                                auto kn2 = il2cpp_class_get_name(fk2);
                                if (!kn2 || strcmp(kn2, "Object") == 0) break;
                                void *iter = nullptr;
                                while (auto f = il2cpp_class_get_fields(fk2, &iter)) {
                                    auto fflags = il2cpp_field_get_flags(f);
                                    if (fflags & FIELD_ATTRIBUTE_STATIC) continue;
                                    auto fname = il2cpp_field_get_name(f);
                                    uint32_t foff = il2cpp_field_get_offset(f);
                                    auto ftype = il2cpp_field_get_type(f);
                                    auto fcls = il2cpp_class_from_type(ftype);
                                    auto tname = fcls ? il2cpp_class_get_name(fcls) : "?";
                                    out << "      0x" << std::hex << foff << " " << tname
                                        << " " << fname;
                                    // Print value for simple types
                                    if (foff + 4 <= ksz) {
                                        if (strcmp(tname, "Int32") == 0 || strcmp(tname, "UInt32") == 0) {
                                            int32_t v = *(int32_t *)(eBase + foff);
                                            out << " = " << std::dec << v;
                                            if (v == curHP) out << " *** MATCH CurHP ***";
                                        } else if (strcmp(tname, "Single") == 0) {
                                            float v = *(float *)(eBase + foff);
                                            out << " = " << std::dec << v;
                                            if (v == (float)curHP) out << " *** MATCH CurHP ***";
                                        } else if (strcmp(tname, "Boolean") == 0 && foff < ksz) {
                                            out << " = " << (eBase[foff] ? "true" : "false");
                                        }
                                    }
                                    out << "\n";
                                }
                                fk2 = il2cpp_class_get_parent(fk2);
                            }
                        }
                    }
                }
            }
        }
    }

    // === 2. Expand m_Handlers array ===
    out << "\n[Object[] m_Handlers]\n";
    if (safe_readable((void *)handlersPtr, 0x20)) {
        auto handArr = reinterpret_cast<Il2CppArray *>(handlersPtr);
        uint64_t hLen = handArr->max_length;
        out << "  Array length = " << std::dec << hLen << "\n";

        auto handData = reinterpret_cast<uint8_t *>(handArr) + 0x20;
        uint64_t hScan = hLen < 32 ? hLen : 32;
        for (uint64_t i = 0; i < hScan; ++i) {
            uint64_t hPtr = *(uint64_t *)(handData + i * 8);
            out << "  [" << std::dec << i << "] ";
            if (!hPtr || !safe_readable((void *)hPtr, 0x10)) {
                out << "null\n";
                continue;
            }
            auto hObj = reinterpret_cast<Il2CppObject *>(hPtr);
            if (hObj->klass) {
                auto hn = il2cpp_class_get_name(hObj->klass);
                auto hns = il2cpp_class_get_namespace(hObj->klass);
                out << (hns && hns[0] ? hns : "") << "::" << (hn ? hn : "?")
                    << " @ 0x" << std::hex << hPtr << "\n";
            } else {
                out << "no klass\n";
            }
        }
    }

    // === 3. Scan Player memory exhaustively for the HP int32 value ===
    // Also try reading memory at deeper offsets that might be
    // value accessor caches or COW buffer data
    out << "\n[Exhaustive Int32 Scan — Player memory for " << std::dec << curHP << "]\n";
    {
        uint32_t pSize = il2cpp_class_instance_size(playerK);
        int matchCount = 0;
        for (uint32_t off = 0x10; off + 4 <= pSize; off += 4) {
            int32_t v = *(int32_t *)(rawBase + off);
            if (v == curHP) {
                out << "  Player+0x" << std::hex << off << " = " << std::dec << v << "\n";
                ++matchCount;
            }
        }
        if (matchCount == 0) out << "  (no matches in Player instance)\n";
    }

    // === 4. Follow 1 level of pointers from Player and scan each ===
    out << "\n[Pointer Chase — 1-deep from Player, scanning for " << std::dec << curHP << "]\n";
    {
        uint32_t pSize = il2cpp_class_instance_size(playerK);
        int totalMatches = 0;
        for (uint32_t off = 0x10; off + 8 <= pSize; off += 8) {
            uint64_t ptr = *(uint64_t *)(rawBase + off);
            if (!safe_readable((void *)ptr, 0x20)) continue;
            auto pObj = reinterpret_cast<Il2CppObject *>(ptr);
            if (!pObj->klass) continue;
            uint32_t subSz = il2cpp_class_instance_size(pObj->klass);
            if (subSz < 0x14 || subSz > 0x1000) continue;
            if (!safe_readable((void *)ptr, subSz)) continue;
            auto sub = reinterpret_cast<uint8_t *>(ptr);
            for (uint32_t s = 0x10; s + 4 <= subSz; s += 4) {
                int32_t v = *(int32_t *)(sub + s);
                if (v == curHP) {
                    auto sn = il2cpp_class_get_name(pObj->klass);
                    out << "  Player+0x" << std::hex << off << " -> "
                        << (sn ? sn : "?") << "+0x" << s
                        << " = " << std::dec << v << "\n";
                    ++totalMatches;
                }
            }
        }
        if (totalMatches == 0) out << "  (no matches at depth 1)\n";
    }

    out.close();
    LOGI("=== PHASE 7 DONE — %s ===", outPath.c_str());
}

// ============================================================
// Phase 8: Position validation (ultra-minimal)
// ONLY reads 12 bytes at 0x320 and 0x7E8 — no full Player scan.
// Two samples with 5s gap to confirm values change with movement.
// ============================================================

static void resolve_offsets_phase8(std::string outDir) {
    LOGI("=== PHASE 8: Position validation ===");
    sleep(35);

    auto dom = il2cpp_domain_get();
    if (!dom) { LOGE("Phase8: domain null"); return; }
    auto thr = il2cpp_thread_attach(dom);
    if (!thr) { LOGE("Phase8: thread attach fail"); return; }

    auto outPath = outDir + "/files/position_trace.txt";
    std::ofstream out(outPath);
    if (!out.is_open()) { LOGE("Phase8: Cannot open %s", outPath.c_str()); return; }

    auto facadeK = find_class_all("COW", "GameFacade");
    if (!facadeK) { out << "Missing GameFacade\n"; out.close(); return; }

    auto clpMethod = il2cpp_class_get_method_from_name(facadeK, "CurrentLocalPlayer", 0);
    if (!clpMethod) { out << "Missing CLP\n"; out.close(); return; }

    Il2CppObject *localPlayer = nullptr;
    for (int attempt = 0; attempt < 180; ++attempt) {
        Il2CppException *exc = nullptr;
        auto r = il2cpp_runtime_invoke(clpMethod, nullptr, nullptr, &exc);
        if (!exc && r) { localPlayer = r; break; }
        sleep(3);
    }
    if (!localPlayer) {
        out << "Phase8: No player\n"; out.close(); return;
    }

    auto rawBase = reinterpret_cast<uint8_t *>(localPlayer);

    out << "// =============================================\n"
        << "// Phantom Phase 8 — Position Validation (v6)\n"
        << "// FF version 1.132.9\n"
        << "// =============================================\n\n"
        << "Player* = 0x" << std::hex << (uint64_t)localPlayer << "\n\n";

    // Sample 1 — only read the two known candidate offsets
    out << "[Sample 1]\n";
    float s1_320x = *(float *)(rawBase + 0x320);
    float s1_320y = *(float *)(rawBase + 0x324);
    float s1_320z = *(float *)(rawBase + 0x328);
    float s1_7e8x = *(float *)(rawBase + 0x7E8);
    float s1_7e8y = *(float *)(rawBase + 0x7EC);
    float s1_7e8z = *(float *)(rawBase + 0x7F0);
    out << "  +0x320 = (" << s1_320x << ", " << s1_320y << ", " << s1_320z << ")\n";
    out << "  +0x7E8 = (" << s1_7e8x << ", " << s1_7e8y << ", " << s1_7e8z << ")\n";

    // Also check HP to confirm player is alive and offsets still work
    uint64_t priPtr = *(uint64_t *)(rawBase + 0x70);
    if (safe_readable((void *)priPtr, 0x20)) {
        auto priBase = reinterpret_cast<uint8_t *>(priPtr);
        uint64_t datasPtr = *(uint64_t *)(priBase + 0x10);
        if (safe_readable((void *)datasPtr, 0x28)) {
            auto arrData = reinterpret_cast<uint8_t *>(datasPtr) + 0x20;
            uint64_t elem0 = *(uint64_t *)(arrData);
            if (safe_readable((void *)elem0, 0x20)) {
                int32_t hp = *(int32_t *)(reinterpret_cast<uint8_t *>(elem0) + 0x18);
                out << "  CurHP (repl[0]) = " << std::dec << hp << "\n";
            }
        }
    }

    sleep(5);

    // Sample 2
    out << "\n[Sample 2 — 5s later]\n";
    float s2_320x = *(float *)(rawBase + 0x320);
    float s2_320y = *(float *)(rawBase + 0x324);
    float s2_320z = *(float *)(rawBase + 0x328);
    float s2_7e8x = *(float *)(rawBase + 0x7E8);
    float s2_7e8y = *(float *)(rawBase + 0x7EC);
    float s2_7e8z = *(float *)(rawBase + 0x7F0);
    out << "  +0x320 = (" << s2_320x << ", " << s2_320y << ", " << s2_320z << ")\n";
    out << "  +0x7E8 = (" << s2_7e8x << ", " << s2_7e8y << ", " << s2_7e8z << ")\n";

    bool moved_320 = (s1_320x != s2_320x || s1_320y != s2_320y || s1_320z != s2_320z);
    bool moved_7e8 = (s1_7e8x != s2_7e8x || s1_7e8y != s2_7e8y || s1_7e8z != s2_7e8z);
    out << "\n  +0x320 changed: " << (moved_320 ? "YES" : "NO") << "\n";
    out << "  +0x7E8 changed: " << (moved_7e8 ? "YES" : "NO") << "\n";

    out.close();
    LOGI("=== PHASE 8 DONE — %s ===", outPath.c_str());
}

// ============================================================

void il2cpp_dump(const char *outDir) {
    LOGI("dumping...");
    size_t size;
    auto domain = il2cpp_domain_get();
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);
    std::stringstream imageOutput;
    for (int i = 0; i < size; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        imageOutput << "// Image " << i << ": " << il2cpp_image_get_name(image) << "\n";
    }
    std::vector<std::string> outPuts;
    if (il2cpp_image_get_class) {
        LOGI("Version greater than 2018.3");
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            imageStr << "\n// Dll : " << il2cpp_image_get_name(image);
            auto classCount = il2cpp_image_get_class_count(image);
            for (int j = 0; j < classCount; ++j) {
                auto klass = il2cpp_image_get_class(image, j);
                auto type = il2cpp_class_get_type(const_cast<Il2CppClass *>(klass));
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.push_back(outPut);
            }
        }
    } else {
        LOGI("Version less than 2018.3");
        auto corlib = il2cpp_get_corlib();
        auto assemblyClass = il2cpp_class_from_name(corlib, "System.Reflection", "Assembly");
        auto assemblyLoad = il2cpp_class_get_method_from_name(assemblyClass, "Load", 1);
        auto assemblyGetTypes = il2cpp_class_get_method_from_name(assemblyClass, "GetTypes", 0);
        if (assemblyLoad && assemblyLoad->methodPointer) {
            LOGI("Assembly::Load: %p", assemblyLoad->methodPointer);
        } else {
            LOGI("miss Assembly::Load");
            return;
        }
        if (assemblyGetTypes && assemblyGetTypes->methodPointer) {
            LOGI("Assembly::GetTypes: %p", assemblyGetTypes->methodPointer);
        } else {
            LOGI("miss Assembly::GetTypes");
            return;
        }
        typedef void *(*Assembly_Load_ftn)(void *, Il2CppString *, void *);
        typedef Il2CppArray *(*Assembly_GetTypes_ftn)(void *, void *);
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            auto image_name = il2cpp_image_get_name(image);
            imageStr << "\n// Dll : " << image_name;
            auto imageName = std::string(image_name);
            auto pos = imageName.rfind('.');
            auto imageNameNoExt = imageName.substr(0, pos);
            auto assemblyFileName = il2cpp_string_new(imageNameNoExt.data());
            auto reflectionAssembly = ((Assembly_Load_ftn) assemblyLoad->methodPointer)(nullptr,
                                                                                        assemblyFileName,
                                                                                        nullptr);
            auto reflectionTypes = ((Assembly_GetTypes_ftn) assemblyGetTypes->methodPointer)(
                    reflectionAssembly, nullptr);
            auto items = reflectionTypes->vector;
            for (int j = 0; j < reflectionTypes->max_length; ++j) {
                auto klass = il2cpp_class_from_system_type((Il2CppReflectionType *) items[j]);
                auto type = il2cpp_class_get_type(klass);
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.push_back(outPut);
            }
        }
    }
    LOGI("write dump file");
    auto outPath = std::string(outDir).append("/files/dump.cs");
    std::ofstream outStream(outPath);
    outStream << imageOutput.str();
    auto count = outPuts.size();
    for (int i = 0; i < count; ++i) {
        outStream << outPuts[i];
    }
    outStream.close();
    LOGI("dump done!");

    resolve_offsets(outDir);
}