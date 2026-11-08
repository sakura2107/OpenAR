#pragma once
#include <rapidjson/document.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>
#include <set>
#include <cstring>
#include "core/IO.h"
namespace ar::core {
// Bound nesting before invoking a recursive parser/schema validator.
inline Result<rapidjson::Document> read_json(const std::filesystem::path &path,
                                             const char *schema = nullptr) {
    auto bytes = read_bytes(path, 4 * 1024 * 1024);
    if (!bytes)
        return bytes.error();
    int depth = 0;
    bool string = false, escaped = false;
    for (auto c : bytes.value()) {
        if (string) {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                string = false;
        } else if (c == '"')
            string = true;
        else if (c == '{' || c == '[') {
            if (++depth > 64)
                return failure(ErrorCode::resource_invalid, "json", "JSON nesting exceeds 64 levels");
        } else if (c == '}' || c == ']')
            --depth;
    }
    rapidjson::Document doc;
    doc.Parse<rapidjson::kParseValidateEncodingFlag>(reinterpret_cast<const char *>(bytes.value().data()),
                                                     bytes.value().size());
    const auto label = wide_to_utf8(path.wstring());
    if (doc.HasParseError())
        return failure(ErrorCode::resource_invalid, "json",
                       label + ": parse error at byte " + std::to_string(doc.GetErrorOffset()));
    std::vector<const rapidjson::Value *> pending{&doc};
    while (!pending.empty()) {
        const auto *value = pending.back();
        pending.pop_back();
        if (value->IsObject()) {
            std::set<std::string> names;
            for (auto it = value->MemberBegin(); it != value->MemberEnd(); ++it) {
                std::string key(it->name.GetString(), it->name.GetStringLength());
                if (key.find('\0') != std::string::npos || !names.insert(key).second)
                    return failure(ErrorCode::resource_invalid, "json",
                                   label + ": duplicate/NUL key: " + key);
                pending.push_back(&it->value);
            }
        } else if (value->IsArray())
            for (const auto &child : value->GetArray())
                pending.push_back(&child);
        else if (value->IsString() && std::memchr(value->GetString(), 0, value->GetStringLength()))
            return failure(ErrorCode::resource_invalid, "json", label + ": NUL in string");
    }
    if (schema) {
        rapidjson::Document definition;
        definition.Parse(schema);
        if (definition.HasParseError())
            return failure(ErrorCode::backend_failed, "schema", "Invalid embedded schema");
        rapidjson::SchemaDocument compiled(definition);
        rapidjson::SchemaValidator validator(compiled);
        if (!doc.Accept(validator)) {
            rapidjson::StringBuffer pointer;
            validator.GetInvalidDocumentPointer().StringifyUriFragment(pointer);
            return failure(ErrorCode::resource_invalid, "schema",
                           label + ": " + pointer.GetString() + " violates " +
                               validator.GetInvalidSchemaKeyword());
        }
    }
    return std::move(doc);
}
} // namespace ar::core
