/*
 * json.h
 *
 *  Created on: Feb 27, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_JSON_H_
#define INCLUDE_MMPILOT_JSON_H_

// Features:
// - Parse JSON from file: read(path)
// - Write pretty JSON to file: write(path, value)  (2-space indent)
// - to_string(value) returns single-line JSON
// - Supports: null (represented as nullptr), true/false, int (<= 2^63-1, >= -2^63),
//   real (double), string (escapes + \uXXXX basic), array, object
// - Value::get<T>() supports ALL integral types (signed/unsigned), floating point, bool, std::string
//
// Notes:
// - "null" is represented as std::shared_ptr<Value>{} (nullptr). This is intentional.
// - Object::operator[] returns nullptr for missing fields (or if field is explicitly null).
// - Object::get<T>(name) throws if missing or null.
// - \uXXXX handling does not combine surrogate pairs.

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <cctype>
#include <limits>
#include <cstring>
#include <cmath>
#include <type_traits>


namespace json {

class Value {
public:
    virtual ~Value() {}

    template<typename T>
    T get();

    template<typename T>
    void get(T& v) { v = get<T>(); }
};

class Bool : public Value {
public:
    bool value = false;
};

class Real : public Value {
public:
    double value = 0;
};

class Integer : public Value {
public:
    int64_t value = 0;
};

class String : public Value {
public:
    std::string value;
};

class Array : public Value {
public:
    std::vector<std::shared_ptr<Value>> values;
};

class Object : public Value {
public:
    std::map<std::string, std::shared_ptr<Value>> fields;

    std::shared_ptr<Value> operator[](const std::string& name) const;

    template<typename T>
    T get(const std::string& name) {
        auto v = (*this)[name];
        if(!v) throw std::runtime_error("json::Object::get(): missing or null field '" + name + "'");
        return v->get<T>();
    }
};

// returns single line
std::string to_string(std::shared_ptr<Value> value);

std::shared_ptr<Value> read(const std::string& path);
void write(const std::string& path, std::shared_ptr<Value> value);

// ------------------------------------------------------------
// Value::get<T>() with full integer type support (signed/unsigned)
// ------------------------------------------------------------
namespace detail {

template<typename T>
static inline T checked_cast_int64_to_integral(int64_t x) {
    static_assert(std::is_integral_v<T> && !std::is_same_v<T,bool>, "integral type required");
    if constexpr (std::is_signed_v<T>) {
        if (x < (int64_t)std::numeric_limits<T>::min() || x > (int64_t)std::numeric_limits<T>::max())
            throw std::runtime_error("json::Value::get<integral>(): out of range");
        return (T)x;
    } else {
        if (x < 0) throw std::runtime_error("json::Value::get<integral>(): negative to unsigned");
        using U64 = std::make_unsigned_t<int64_t>;
        const U64 ux = (U64)x;
        if (ux > (U64)std::numeric_limits<T>::max())
            throw std::runtime_error("json::Value::get<integral>(): out of range");
        return (T)ux;
    }
}

template<typename T>
static inline T checked_cast_double_to_integral(double d) {
    static_assert(std::is_integral_v<T> && !std::is_same_v<T,bool>, "integral type required");
    if (!std::isfinite(d)) throw std::runtime_error("json::Value::get<integral>(): NaN/Inf");
    const double td = std::trunc(d);
    if (td != d) throw std::runtime_error("json::Value::get<integral>(): non-integer real");

    if constexpr (std::is_signed_v<T>) {
        const double lo = (double)std::numeric_limits<T>::min();
        const double hi = (double)std::numeric_limits<T>::max();
        if (d < lo || d > hi) throw std::runtime_error("json::Value::get<integral>(): out of range");
        return (T)td;
    } else {
        if (d < 0.0) throw std::runtime_error("json::Value::get<integral>(): negative to unsigned");
        const double hi = (double)std::numeric_limits<T>::max();
        if (d > hi) throw std::runtime_error("json::Value::get<integral>(): out of range");
        return (T)td;
    }
}

} // namespace detail

template<typename T>
T Value::get() {
    // bool
    if constexpr (std::is_same_v<T, bool>) {
        if (auto b = dynamic_cast<Bool*>(this)) return b->value;
        throw std::runtime_error("json::Value::get<bool>(): not a bool");
    }
    // string
    else if constexpr (std::is_same_v<T, std::string>) {
        if (auto s = dynamic_cast<String*>(this)) return s->value;
        throw std::runtime_error("json::Value::get<std::string>(): not a string");
    }
    // floating point
    else if constexpr (std::is_floating_point_v<T>) {
        if (auto r = dynamic_cast<Real*>(this)) return (T)r->value;
        if (auto i = dynamic_cast<Integer*>(this)) return (T)i->value;
        throw std::runtime_error("json::Value::get<floating>(): not a number");
    }
    // integral (all signed/unsigned integer types except bool)
    else if constexpr (std::is_integral_v<T>) {
        if (auto i = dynamic_cast<Integer*>(this)) {
            return detail::checked_cast_int64_to_integral<T>(i->value);
        }
        if (auto r = dynamic_cast<Real*>(this)) {
            return detail::checked_cast_double_to_integral<T>(r->value);
        }
        throw std::runtime_error("json::Value::get<integral>(): not a number");
    }
    else {
        static_assert(sizeof(T) == 0, "json::Value::get<T>(): unsupported T");
    }
}

// ------------------------------------------------------------
// Object access
// ------------------------------------------------------------
std::shared_ptr<Value> Object::operator[](const std::string& name) const {
    auto it = fields.find(name);
    if(it == fields.end()) return {};
    return it->second;
}

// ------------------------------------------------------------
// Parser
// ------------------------------------------------------------
namespace detail {

static inline bool is_ws(char c) {
    return c==' ' || c=='\n' || c=='\r' || c=='\t';
}

class Parser {
public:
    explicit Parser(std::string s) : src(std::move(s)) {}

    std::shared_ptr<Value> parse_root() {
        skip_ws();
        auto v = parse_value();
        skip_ws();
        if(pos != src.size()) throw std::runtime_error(err("trailing characters"));
        return v;
    }

private:
    std::string src;
    size_t pos = 0;

    std::string err(const std::string& msg) const {
        return "json parse error at pos " + std::to_string(pos) + ": " + msg;
    }

    void skip_ws() {
        while(pos < src.size() && is_ws(src[pos])) ++pos;
    }

    char peek() const {
        return (pos < src.size()) ? src[pos] : '\0';
    }

    char getc() {
        if(pos >= src.size()) throw std::runtime_error(err("unexpected end of input"));
        return src[pos++];
    }

    void expect(char c) {
        char g = getc();
        if(g != c) throw std::runtime_error(err(std::string("expected '") + c + "', got '" + g + "'"));
    }

    bool match_kw(const char* kw) {
        const size_t n = std::strlen(kw);
        if(pos + n > src.size()) return false;
        for(size_t i=0;i<n;++i) if(src[pos+i] != kw[i]) return false;
        pos += n;
        return true;
    }

    std::shared_ptr<Value> parse_value() {
        skip_ws();
        const char c = peek();

        if(c == '{') return parse_object();
        if(c == '[') return parse_array();
        if(c == '"') return parse_string();
        if(c == '-' || std::isdigit((unsigned char)c)) return parse_number();

        if(match_kw("true"))  { auto b = std::make_shared<Bool>(); b->value = true;  return b; }
        if(match_kw("false")) { auto b = std::make_shared<Bool>(); b->value = false; return b; }
        if(match_kw("null"))  { return {}; }

        throw std::runtime_error(err("unexpected token"));
    }

    std::shared_ptr<Value> parse_object() {
        expect('{');
        auto obj = std::make_shared<Object>();
        skip_ws();
        if(peek() == '}') { getc(); return obj; }

        for(;;) {
            skip_ws();
            auto keyv = parse_string();
            auto key = std::dynamic_pointer_cast<String>(keyv);
            if(!key) throw std::runtime_error(err("object key must be a string"));

            skip_ws();
            expect(':');
            skip_ws();

            obj->fields[key->value] = parse_value();

            skip_ws();
            const char c = getc();
            if(c == '}') break;
            if(c != ',') throw std::runtime_error(err("expected ',' or '}'"));
        }
        return obj;
    }

    std::shared_ptr<Value> parse_array() {
        expect('[');
        auto arr = std::make_shared<Array>();
        skip_ws();
        if(peek() == ']') { getc(); return arr; }

        for(;;) {
            skip_ws();
            arr->values.push_back(parse_value());
            skip_ws();
            const char c = getc();
            if(c == ']') break;
            if(c != ',') throw std::runtime_error(err("expected ',' or ']'"));
        }
        return arr;
    }

    static inline int hex_val(char c) {
        if(c>='0'&&c<='9') return c - '0';
        if(c>='a'&&c<='f') return 10 + (c - 'a');
        if(c>='A'&&c<='F') return 10 + (c - 'A');
        return -1;
    }

    static void append_utf8(std::string& out, uint32_t cp) {
        if(cp <= 0x7F) {
            out.push_back(char(cp));
        } else if(cp <= 0x7FF) {
            out.push_back(char(0xC0 | ((cp >> 6) & 0x1F)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        } else if(cp <= 0xFFFF) {
            out.push_back(char(0xE0 | ((cp >> 12) & 0x0F)));
            out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(char(0xF0 | ((cp >> 18) & 0x07)));
            out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        }
    }

    std::shared_ptr<Value> parse_string() {
        expect('"');
        auto s = std::make_shared<String>();
        std::string out;

        for(;;) {
            char c = getc();
            if(c == '"') break;
            if((unsigned char)c < 0x20) throw std::runtime_error(err("control character in string"));

            if(c == '\\') {
                char e = getc();
                switch(e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        uint32_t cp = 0;
                        for(int i=0;i<4;++i) {
                            char h = getc();
                            int v = hex_val(h);
                            if(v < 0) throw std::runtime_error(err("invalid \\uXXXX escape"));
                            cp = (cp << 4) | (uint32_t)v;
                        }
                        // NOTE: surrogate pairs not combined
                        append_utf8(out, cp);
                        break;
                    }
                    default:
                        throw std::runtime_error(err("invalid escape sequence"));
                }
            } else {
                out.push_back(c);
            }
        }

        s->value = std::move(out);
        return s;
    }

    std::shared_ptr<Value> parse_number() {
        const size_t start = pos;

        if(peek() == '-') ++pos;

        if(peek() == '0') {
            ++pos;
        } else {
            if(!std::isdigit((unsigned char)peek())) throw std::runtime_error(err("invalid number"));
            while(std::isdigit((unsigned char)peek())) ++pos;
        }

        bool is_real = false;

        if(peek() == '.') {
            is_real = true;
            ++pos;
            if(!std::isdigit((unsigned char)peek())) throw std::runtime_error(err("invalid fraction"));
            while(std::isdigit((unsigned char)peek())) ++pos;
        }

        if(peek() == 'e' || peek() == 'E') {
            is_real = true;
            ++pos;
            if(peek() == '+' || peek() == '-') ++pos;
            if(!std::isdigit((unsigned char)peek())) throw std::runtime_error(err("invalid exponent"));
            while(std::isdigit((unsigned char)peek())) ++pos;
        }

        const std::string token = src.substr(start, pos - start);

        if(!is_real) {
            // strict int64 parse with overflow/underflow detection
            int64_t v = 0;
            bool neg = false;
            size_t i = 0;

            if(token[i] == '-') { neg = true; ++i; }
            if(i >= token.size()) throw std::runtime_error(err("invalid integer"));

            for(; i < token.size(); ++i) {
                char c = token[i];
                if(!std::isdigit((unsigned char)c)) throw std::runtime_error(err("invalid integer"));
                int digit = c - '0';

                if(!neg) {
                    if(v > (std::numeric_limits<int64_t>::max() - digit) / 10)
                        throw std::runtime_error(err("integer overflow"));
                    v = v * 10 + digit;
                } else {
                    if(v < (std::numeric_limits<int64_t>::min() + digit) / 10)
                        throw std::runtime_error(err("integer underflow"));
                    v = v * 10 - digit;
                }
            }

            auto iv = std::make_shared<Integer>();
            iv->value = v;
            return iv;
        } else {
            char* endp = nullptr;
            const double d = std::strtod(token.c_str(), &endp);
            if(endp == token.c_str() || *endp != '\0')
                throw std::runtime_error(err("invalid real"));
            auto rv = std::make_shared<Real>();
            rv->value = d;
            return rv;
        }
    }
};

static inline std::string slurp_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if(!f) throw std::runtime_error("json::read(): cannot open file: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace detail

// ------------------------------------------------------------
// Single-line serializer: to_string()
// ------------------------------------------------------------
namespace detail {

static inline void write_escaped_str(std::string& out, const std::string& s) {
    out.push_back('"');
    for(unsigned char uc : s) {
        char c = (char)uc;
        switch(c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '/':  out += "\\/";  break; // optional but ok
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if(uc < 0x20) {
                    static const char* hex = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(hex[(uc >> 4) & 0xF]);
                    out.push_back(hex[(uc     ) & 0xF]);
                } else {
                    out.push_back(c);
                }
        }
    }
    out.push_back('"');
}

static inline std::string double_to_string(double d) {
    std::ostringstream ss;
    ss.setf(std::ios::fmtflags(0), std::ios::floatfield);
    ss.precision(17);
    ss << d;
    return ss.str();
}

static void dump_single(std::string& out, const std::shared_ptr<Value>& v) {
    if(!v) { out += "null"; return; }

    if(auto b = std::dynamic_pointer_cast<Bool>(v))    { out += (b->value ? "true" : "false"); return; }
    if(auto i = std::dynamic_pointer_cast<Integer>(v)) { out += std::to_string(i->value); return; }
    if(auto r = std::dynamic_pointer_cast<Real>(v))    { out += double_to_string(r->value); return; }
    if(auto s = std::dynamic_pointer_cast<String>(v))  { write_escaped_str(out, s->value); return; }

    if(auto a = std::dynamic_pointer_cast<Array>(v)) {
        out.push_back('[');
        for(size_t k=0;k<a->values.size();++k) {
            if(k) out.push_back(',');
            dump_single(out, a->values[k]);
        }
        out.push_back(']');
        return;
    }

    if(auto o = std::dynamic_pointer_cast<Object>(v)) {
        out.push_back('{');
        bool first = true;
        for(const auto& kv : o->fields) {
            if(!first) out.push_back(',');
            first = false;
            write_escaped_str(out, kv.first);
            out.push_back(':');
            dump_single(out, kv.second);
        }
        out.push_back('}');
        return;
    }

    throw std::runtime_error("json::to_string(): unknown Value subclass");
}

// Pretty serializer for write()
static inline void write_escaped_stream(std::ostream& os, const std::string& s) {
    os.put('"');
    for (unsigned char uc : s) {
        char c = (char)uc;
        switch (c) {
            case '"':  os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\b': os << "\\b";  break;
            case '\f': os << "\\f";  break;
            case '\n': os << "\\n";  break;
            case '\r': os << "\\r";  break;
            case '\t': os << "\\t";  break;
            default:
                if (uc < 0x20) {
                    static const char* hex = "0123456789abcdef";
                    os << "\\u00" << hex[(uc >> 4) & 0xF] << hex[uc & 0xF];
                } else {
                    os.put(c);
                }
        }
    }
    os.put('"');
}

static inline void indent(std::ostream& os, int n) {
    for(int i=0;i<n;i++) os.put(' ');
}

static void dump_pretty(std::ostream& os, const std::shared_ptr<Value>& v, int ind, int step) {
    if(!v) { os << "null"; return; }

    if(auto b = std::dynamic_pointer_cast<Bool>(v))    { os << (b->value ? "true" : "false"); return; }
    if(auto i = std::dynamic_pointer_cast<Integer>(v)) { os << i->value; return; }
    if(auto r = std::dynamic_pointer_cast<Real>(v))    { os << double_to_string(r->value); return; }
    if(auto s = std::dynamic_pointer_cast<String>(v))  { write_escaped_stream(os, s->value); return; }

    if(auto a = std::dynamic_pointer_cast<Array>(v)) {
        os << "[";
        if(!a->values.empty()) {
            os << "\n";
            for(size_t k=0;k<a->values.size();++k) {
                indent(os, ind + step);
                dump_pretty(os, a->values[k], ind + step, step);
                if(k + 1 < a->values.size()) os << ",";
                os << "\n";
            }
            indent(os, ind);
        }
        os << "]";
        return;
    }

    if(auto o = std::dynamic_pointer_cast<Object>(v)) {
        os << "{";
        if(!o->fields.empty()) {
            os << "\n";
            size_t k = 0;
            for(const auto& kv : o->fields) {
                indent(os, ind + step);
                write_escaped_stream(os, kv.first);
                os << ": ";
                dump_pretty(os, kv.second, ind + step, step);
                if(++k < o->fields.size()) os << ",";
                os << "\n";
            }
            indent(os, ind);
        }
        os << "}";
        return;
    }

    throw std::runtime_error("json::write(): unknown Value subclass");
}

} // namespace detail

std::string to_string(std::shared_ptr<Value> value) {
    std::string out;
    out.reserve(256);
    detail::dump_single(out, value);
    return out;
}

std::shared_ptr<Value> read(const std::string& path) {
    const std::string s = detail::slurp_file(path);
    detail::Parser p(s);
    return p.parse_root();
}

void write(const std::string& path, std::shared_ptr<Value> value) {
    std::ofstream f(path, std::ios::binary);
    if(!f) throw std::runtime_error("json::write(): cannot open file: " + path);

    // Pretty output: 2 spaces
    detail::dump_pretty(f, value, /*ind=*/0, /*step=*/2);
    f.put('\n');

    if(!f) throw std::runtime_error("json::write(): failed writing file: " + path);
}

// Optional explicit instantiations (helps if you split into multiple TUs)
template bool Value::get<bool>();
template std::string Value::get<std::string>();
template float Value::get<float>();
template double Value::get<double>();
template long double Value::get<long double>();
template int8_t Value::get<int8_t>();
template uint8_t Value::get<uint8_t>();
template int16_t Value::get<int16_t>();
template uint16_t Value::get<uint16_t>();
template int32_t Value::get<int32_t>();
template uint32_t Value::get<uint32_t>();
template int64_t Value::get<int64_t>();
template uint64_t Value::get<uint64_t>();
template int Value::get<int>();
template unsigned Value::get<unsigned>();
template long Value::get<long>();
template unsigned long Value::get<unsigned long>();
template long long Value::get<long long>();
template unsigned long long Value::get<unsigned long long>();
template size_t Value::get<size_t>();

} // namespace json

#endif /* INCLUDE_MMPILOT_JSON_H_ */
