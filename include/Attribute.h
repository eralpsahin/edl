#ifndef _ATTRIBUTE_H_
#define _ATTRIBUTE_H_
#include <string>
#include <vector>
namespace pdg {
class Attribute {
 private:
  bool in;
  bool out;
  bool isptr;
  bool readonly;
  bool string;
  bool user_check;
  std::string size;
  std::string count;

  // Private function to use in dump()
  std::string convertToString(std::vector<std::string> attrs) {
    if (attrs.size() == 1) return attrs[0];  // Return the only attribute

    // At least 2 attributes
    std::string res = "";
    for (unsigned int i = 0; i < attrs.size() - 1; i += 1) {
      res += attrs[i] + ", ";
    }
    res += attrs.back();
    return res;
  }

 public:
  Attribute()
      : in(0), out(0), isptr(0), readonly(0), string(0), size(""), count(""){};

  // Set attributes
  void setIn() { in = 1; }
  void setOut() { out = 1; }
  void setIsPtr() { isptr = 1; }
  void setReadOnly() { readonly = 1; }
  void setString() { string = 1; }
  void setSize(std::string val) { size = val; }
  void setCount(std::string val) { count = val; }

  // Dump the attributes
  std::string dump() {
    std::vector<std::string> attrs;
    if (string || in) {  // string has to have [in] attribute
      in = 1;
      attrs.push_back("in");
    }
    if (out) {
      attrs.push_back("out");
    }
    if (!in && !out) {
      user_check = 1;
      attrs.push_back("user_check");
    }
    if (isptr) {
      attrs.push_back("isptr");
    }
    if (readonly) {
      // First rule is readonly can not be used with out
      assert(!out && "Readonly cannot be used with pout]");
      assert(isptr && "Readonly cannot be used without [isptr]");
      attrs.push_back("readonly");
    }
    if (string) {  // If there is a string attribute there should not be size or
                   // count
      attrs.push_back("string");
    } else if (in || out){

      // TODO: handle when size or count is not a literal number
      if (size.length() != 0) attrs.push_back("size=" + size);
      if (count.length() != 0 && count != "1")
        attrs.push_back("count=" + count);  // Count 1 is what EDL assumes
    }
    return "[" + convertToString(attrs) + "]";
  }
};
}  // namespace pdg
#endif