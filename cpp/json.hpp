#ifndef JSON_HPP_
#define JSON_HPP_

// A simple memory efficient JSON parser, uses only about 2X memory of the size of JSON string.
// It could handle at most 4G JSON data and 512M items per JSON data type, i.e., string, number,
// bool, array or object.

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <deque>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace json {

class StringPiece {
  public:
    StringPiece(const std::string& str) : ptr_(str.c_str()), len_(str.size()) {}

    bool operator== (StringPiece other) const {
        if (len_ != other.len_) return false;
        for (int i = 0; i < len_; i++) {
            if (ptr_[i] != other.ptr_[i]) return false;
        }
        return true;
    }

  private:
    const char* ptr_ = nullptr;
    uint32_t len_ = 0;
};

namespace {

const int VALUE_INDEX_BITS = 29;
const uint32_t VALUE_INDEX_MASK = (1 << VALUE_INDEX_BITS) - 1;
const uint32_t MAX_DATA_SIZE = -1;
const char TRUE_VALUE[] = "true";
const char FALSE_VALUE[] = "false";

}  // namespace

class Parser;

class Value {
  public:
    enum Type {
        STRING = 1,
        NUMBER = 2,
        BOOL = 3,
        ARRAY = 4,
        OBJECT = 5
    };

    Value() {}

    bool valid() const { return type() > 0; }

    Type type() const { return (enum Type)(value_ >> VALUE_INDEX_BITS); }

  private:
    friend class Parser;

    Value(enum Type type, uint32_t index) {
        assert(index == (index & VALUE_INDEX_MASK));
        value_ = (type << VALUE_INDEX_BITS) | index;
    }

    uint32_t index() const { return value_ & VALUE_INDEX_MASK; }

    uint32_t value_ = 0;
};

class Parser {
  public:
    Parser() {}

    ~Parser() {
        if (mapped_ != nullptr) munmap(mapped_, mapped_size_);
        if (fd_ >= 0) close(fd_);
    }

    Value ParseFromFile(const char* filename) {
        fd_ = open(filename, 0);
        if (fd_ < 0) {
            printf("Failed to open '%s'.\n", filename);
            return Value();
        }
        struct stat st;
        if (fstat(fd_, &st) != 0) {
            printf("Failed to stat '%s'.\n", filename);
            return Value();
        }
        mapped_ = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd_, 0);
        if (mapped_ == MAP_FAILED) {
            printf("Failed to mmap '%s': %s\n", filename, strerror(errno));
            return Value();
        }
        mapped_size_ = st.st_size;
        assert(mapped_size_ <= MAX_DATA_SIZE); 
        return ParseFromArray((const char*)mapped_, mapped_size_);
    }

    Value ParseFromString(const std::string& str) {
        assert(str.size() <= MAX_DATA_SIZE); 
        return ParseFromArray(str.c_str(), str.size());
    }

    Value ParseFromArray(const char* ptr, uint32_t size) {
        assert(ptr_ == nullptr);
        ptr_ = ptr;
        size_ = size;
        offset_ = 0;
        Value value = ParseValue();
        if (value.valid() && size_ > offset_) {
            printf("superfluous data(%d bytes) in the end.\n", size_ - offset_);
        }
        return value;
    }

    std::string AsString(Value value) const {
        assert(value.type() == Value::STRING);
        return AsString(strs_.Get(value.index()));
    }

    double AsNumber(Value value) const {
        assert(value.type() == Value::NUMBER);
        return nums_.Get(value.index());
    }

    bool AsBool(Value value) const {
        assert(value.type() == Value::BOOL);
        return value.index() != 0;
    }

    uint32_t ArraySize(Value value) const {
        assert(value.type() == Value::ARRAY);
        return arrays_.Get(value.index())->size();
    }

    Value ArrayElem(Value value, uint32_t index) const {
        assert(value.type() == Value::ARRAY);
        return arrays_.Get(value.index())->at(index);
    }

    uint32_t ObjectSize(Value value) const {
        assert(value.type() == Value::OBJECT);
        return objects_.Get(value.index())->size();
    }

    Value ObjectProp(Value value, const std::string& name) const {
        assert(value.type() == Value::OBJECT);
        const auto& obj = objects_.Get(value.index());
        for (const auto& prop : *obj) {
            if (prop.first == name) return prop.second;
        }
        return Value();
    }

  private:
    typedef std::pair<uint32_t, uint32_t> String;  // <offset, len>
    typedef std::vector<Value> Array;
    typedef std::vector<std::pair<StringPiece, Value>> Object;

    template <typename T>
    class Arena {
      public:
        Arena() {}

        const T& Get(uint32_t i) const { return mem_.at(i); }

        uint32_t Add(T&& v) {
            const uint32_t index = mem_.size();
            assert(index <= VALUE_INDEX_MASK);
            mem_.push_back(std::move(v));
            return index;
        }

      private:
        std::deque<T> mem_;
    };

    char NextNonSpaceChar() {
        while (offset_ < size_) {
            const char ch = ptr_[offset_];
            if (isspace(ch)) {
                offset_++;
                if (ch == '\n') {
                    line_++;
                    col_ = 1;
                } else {
                    col_ ++;
                }
            } else {
                return ch;
            }
        }
        return '\0';
    }

    void PrintUnexpectedCh(char ch, const char* prefix) {
        if (ch == '\0') {
            printf("%s, reached EOF!\n", prefix);
        } else {
            printf("%s, found '%c' at line %d, col %d.\n", prefix, ch, line_, col_);
        }
    }

    Value ParseValue() {
        switch (NextNonSpaceChar()) {
            case '\0':
                printf("Unexpected EOF!\n");
                return Value();
            case '[':
                offset_++;
                col_++;
                {
                    std::unique_ptr<Array> array(new Array);
                    while (true) {
                        const Value value = ParseValue();
                        if (!value.valid()) return value;
                        array->push_back(value);
                        const char next_ch = NextNonSpaceChar();
                        if (next_ch == ',') {
                            offset_++;
                            col_++;
                        } else if (next_ch == ']') {
                            offset_++;
                            col_++;
                            return Value(Value::ARRAY, arrays_.Add(std::move(array)));
                        } else {
                            PrintUnexpectedCh(next_ch, "Expecting ',' or ']' parsing array");
                            return Value();
                        }
                    }
                }
            case '{':
                offset_++;
                col_++;
                {
                    std::unique_ptr<Object> object(new Object);
                    while (true) {
                        // Parse key.
                        char next_ch = NextNonSpaceChar();
                        if (next_ch != '"') {
                            PrintUnexpectedCh(next_ch, "Expecting '\"' parsing object key");
                            return Value();
                        }
                        offset_++;
                        col_++;
                        const String key = ParseString();
                        if (!IsValidString(key)) return Value();
                        // Parse ':'.
                        next_ch = NextNonSpaceChar();
                        if (next_ch != ':') {
                            PrintUnexpectedCh(next_ch, "Expecting ':' parsing object");
                            return Value();
                        }
                        offset_++;
                        col_++;
                        // Parse value.
                        const Value value = ParseValue();
                        if (!value.valid()) return value;
                        object->push_back(std::make_pair(AsStringPiece(AsString(key)), value));
                        // Next or end.
                        next_ch = NextNonSpaceChar();
                        if (next_ch == ',') {
                            offset_++;
                            col_++;
                        } else if (next_ch == '}') {
                            offset_++;
                            col_++;
                            return Value(Value::OBJECT, objects_.Add(std::move(object)));
                        } else {
                            PrintUnexpectedCh(next_ch, "Expecting ',' or '}' parsing object");
                            return Value();
                        }
                    }
                }
                break;
            case '"':
                offset_++;
                col_++;
                return AddString(ParseString());
        }
        return ParseBoolOrNumber();
    }

    String ParseString() {
        bool escaping = false;
        for (int i = offset_; i < size_; i++) {
            if (escaping) {
                escaping = false;
                continue;
            }
            switch (ptr_[i]) {
                case '\\':
                    escaping = true;
                    break;
                case '"':
                    {
                        const String str = std::make_pair(offset_, i - offset_);
                        offset_ = i + 1;
                        return str;
                    }
            }
        }
        printf("Unexpected EOF parsing string!\n");
        return String(size_, 0);
    }

    bool IsValidString(String str) {
        return str.first < size_;
    }

    Value ParseBoolOrNumber() {
        if (strncmp(ptr_ + offset_, TRUE_VALUE, sizeof(TRUE_VALUE) - 1) == 0) {
            return Value(Value::BOOL, 1);
        } else if (strncmp(ptr_ + offset_, FALSE_VALUE, sizeof(FALSE_VALUE) - 1) == 0) {
            return Value(Value::BOOL, 0);
        }
        char* end_ptr = nullptr;
        double num = strtod(ptr_ + offset_, &end_ptr);
        if (end_ptr == ptr_) {
            PrintUnexpectedCh(ptr_[offset_], "Expecting digit parsing number");
            return Value();
        }
        offset_ = end_ptr - ptr_;
        return Value(Value::NUMBER, nums_.Add(std::move(num)));
    }

    Value AddString(String str) {
        if (!IsValidString(str)) return Value();
        return Value(Value::STRING, strs_.Add(std::move(str)));
    }

    std::string AsString(String s) const {
        const char* ptr = ptr_ + s.first;
        std::string str;
        bool escaping = false;
        for (int i = 0; i < s.second; i++) {
            if (escaping) {
                switch (ptr[i]) {
                    case 't':
                        str += '\t';
                        break;
                    case 'r':
                        str += '\r';
                        break;
                    case 'n':
                        str += '\n';
                        break;
                    default:
                        str += ptr[i];
                }
                escaping = false;
            } else {
                switch (ptr[i]) {
                    case '\\':
                        escaping = true;
                        break;
                    case '"':
                        assert(false);
                    default:
                        str += ptr[i]; 
                }
            }
        }
        assert(!escaping);
        return str;
    }

    StringPiece AsStringPiece(const std::string& str) {
        auto iter = unique_strs_.find(str);
        if (iter == unique_strs_.end()) {
            iter = unique_strs_.insert(str).first;
        }
        return *iter;
    }

    std::set<std::string> unique_strs_;
    Arena<String> strs_;
    Arena<double> nums_;
    Arena<std::unique_ptr<Array>> arrays_;
    Arena<std::unique_ptr<Object>> objects_;
    int fd_ = -1;
    void* mapped_ = nullptr;
    size_t mapped_size_ = 0;
    const char* ptr_ = nullptr;
    uint32_t size_ = 0, offset_ = 0;
    int line_ = 1, col_ = 1;
};

}  // namespace json

#endif  // JSON_HPP_
