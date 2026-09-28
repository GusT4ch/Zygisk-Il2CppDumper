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