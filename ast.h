#pragma once

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <deque>

#define DEFAULT 0
#define PRINT_VALUE 1
#define CALCULATE_VALUE 2
#define GLOBAL 3
#define FIRST 4
#define SECOND 5
#define TYPE_CHECK 6
#define STORE 7

#define CONSTANT 0
#define VARIABLE 1
#define FUNCTION 2
#define ARRAY 3

union vals {
  std::string* var;
  int val;
};

struct tagUnion {
  int tag;
  vals value;
};

extern std::vector<std::map<std::string, tagUnion>*> symbol_tables;
extern int var_idx;
extern int cal_num;
extern std::vector<int> alloc_byte;
extern int alloc;
extern int if_cnt;
extern int cur_if_cnt;
extern bool callInst;
extern int maxParam;
extern std::vector<int> maxParams;
extern std::vector<int> arrayParams;
extern std::vector<int> arrayParams2;
extern std::vector<int> curArrayParams;
extern std::vector<int> arrayContents;
extern int arrayInitCnt;
extern bool isExp;
extern int ptridx;
extern std::string curIdent;
extern void storeArray(int option, int value);

class BaseAST {
 public:
  static int temp;
  virtual ~BaseAST() = default;

  virtual void Dump(int option) const = 0;
};

class CompUnitAST : public BaseAST {
 public:
  std::unique_ptr<std::vector<BaseAST*>> declsAndDefs;

  void Dump(int option) const override {
    std::cout << "decl @getint(): i32" << std::endl;
    std::cout << "decl @getch(): i32" << std::endl;
    std::cout << "decl @getarray(*i32): i32" << std::endl;
    std::cout << "decl @putint(i32)" << std::endl;
    std::cout << "decl @putch(i32)" << std::endl;
    std::cout << "decl @putarray(i32, *i32)" << std::endl;
    std::cout << "decl @starttime()" << std::endl;
    std::cout << "decl @stoptime()" << std::endl << std::endl;
    
    std::map<std::string, tagUnion>* symbol_table = new std::map<std::string, tagUnion>;
    symbol_tables.push_back(symbol_table);
    std::string func[8] = {"getint", "getch", "getarray", "putint", "putch", "putarray", "starttime", "stoptime"};
    std::string funcType[8] = {"int", "int", "int", "void", "void", "void", "void", "void"};
    for (int i = 0; i < 8; i++){
      tagUnion val;
      val.tag = FUNCTION;
      val.value.var = new std::string(funcType[i]);
      (*(symbol_tables.back()))[func[i]] = val;
    }

    std::vector<BaseAST*>::const_iterator i;
    for(i = (*declsAndDefs).begin(); i != (*declsAndDefs).end(); i++){
      (*i)->Dump(GLOBAL);
    }
    symbol_tables.pop_back();
  }
};

class FuncDefAST : public BaseAST {
 public:
  std::string func_type;
  std::string ident;
  std::unique_ptr<BaseAST> funcFParam;
  std::unique_ptr<std::vector<BaseAST*>> funcFParams;
  std::unique_ptr<BaseAST> block;
  bool params;

  void Dump(int option) const override {
    tagUnion val;
    val.tag = FUNCTION;
    val.value.var = new std::string(func_type);
    (*(symbol_tables.back()))[ident] = val;

    std::cout << "fun @";
    std::cout << ident << "(";
    if (params){
      std::map<std::string, tagUnion>* symbol_table = new std::map<std::string, tagUnion>;
      symbol_tables.push_back(symbol_table);
      funcFParam->Dump(FIRST);
      std::vector<BaseAST*>::const_iterator i;
      for(i = (*funcFParams).begin(); i != (*funcFParams).end(); i++){
        std::cout << ", ";
        (*i)->Dump(FIRST);
      }
    }
    std::cout << ")";
    std::cout << (!func_type.compare("int") ? ": i32" : "");
    std::cout << " {" << std::endl;
    std::cout << "%entry:" << std::endl;
    if (params){
      funcFParam->Dump(SECOND);
      std::vector<BaseAST*>::const_iterator i;
      for(i = (*funcFParams).begin(); i != (*funcFParams).end(); i++){
        (*i)->Dump(SECOND);
      }
    }
    block->Dump(DEFAULT);
    std::cout << (!func_type.compare("void") ? "  ret\n" : "");
    std::cout << (!func_type.compare("int") ? "  jump %then0\n" : "");
    if (std::cout.fail()){
      std::cout.clear();
    }
    std::cout << "}" << std::endl << std::endl;
    if (params){
      symbol_tables.pop_back();
    }
    int s = (alloc + ptridx + BaseAST::temp) * 4;
    if (callInst){
      s += 4;
    }
    if (maxParam > 8){
      s += ((maxParam - 8) * 4);
    }
    if (s % 16){
      s = (s|15) + 1;
    }
    if (callInst){
      s -= 1;
    }
    alloc_byte.push_back(s);
    maxParams.push_back(maxParam - 8);
    alloc = 0;
    BaseAST::temp = 0;
    callInst = false;
    maxParam = 0;
  }
};

class FuncFParamAST : public BaseAST {
 public:
  std::string bType;
  std::string ident;

  void Dump(int option) const override {
    if (option == FIRST){
      std::string var_name = ident + "_" + std::to_string(var_idx);
      var_idx++;

      tagUnion val;
      val.tag = VARIABLE;
      val.value.var = new std::string("%" + var_name);
      (*(symbol_tables.back()))[ident] = val;

      std::cout << "@" << var_name << ": " << (!bType.compare("int") ? "i32" : "");
    } else {
      std::string var_name;
      std::vector<std::map<std::string, tagUnion>*>::const_reverse_iterator i;
      for(i = symbol_tables.rbegin(); i != symbol_tables.rend(); i++){
        if ((*(*i)).find(ident) != (*(*i)).end()) {
          if ((*(*i))[ident].tag != VARIABLE){
            continue;
          }
          var_name = *((*(*i))[ident].value.var);
          break;
        }
      }
      std::cout << "  " << var_name << " = alloc i32" << std::endl;
      std::cout << "  store @" << var_name.substr(1) << ", " << var_name << std::endl;
      alloc++;
    }
  }
};

class FuncFArrayParamAST : public BaseAST {
 public:
  std::string bType;
  std::string ident;
  std::unique_ptr<std::vector<BaseAST*>> constExps;

  void Dump(int option) const override {
    if (option == FIRST){
      std::string var_name = ident + "_" + std::to_string(var_idx);
      var_idx++;

      tagUnion val;
      val.tag = ARRAY;
      std::string name = std::to_string((*constExps).size() + 1) + "%" + var_name;
      val.value.var = new std::string(name);
      (*(symbol_tables.back()))[ident] = val;


      std::cout << "@" << var_name << ": *";
      int exp_cnt = (*constExps).size();
      for (int i = 0; i < exp_cnt; i++){
        std::cout << "[";
      }
      std::cout << "i32";
      std::vector<BaseAST*>::const_reverse_iterator j;
      for(j = (*constExps).rbegin(); j != (*constExps).rend(); j++){
        (*j)->Dump(CALCULATE_VALUE);
        std::cout << ", " << cal_num << "]";
      }
    } else {
      std::string var_name;
      std::vector<std::map<std::string, tagUnion>*>::const_reverse_iterator i;
      for(i = symbol_tables.rbegin(); i != symbol_tables.rend(); i++){
        if ((*(*i)).find(ident) != (*(*i)).end()) {
          if ((*(*i))[ident].tag != ARRAY){
            continue;
          }
          var_name = *((*(*i))[ident].value.var);
          break;
        }
      }

      std::cout << "  " << var_name.substr(1) << " = alloc *";
      alloc++;
      int exp_cnt = (*constExps).size();
      for (int i = 0; i < exp_cnt; i++){
        std::cout << "[";
      }
      std::cout << "i32";
      std::vector<BaseAST*>::const_reverse_iterator j;
      for(j = (*constExps).rbegin(); j != (*constExps).rend(); j++){
        (*j)->Dump(CALCULATE_VALUE);
        std::cout << ", " << cal_num << "]";
      }
      std::cout << std::endl;
      std::cout << "  store @" << var_name.substr(2) << ", " << var_name.substr(1) << std::endl;
    }
  }
};

class DeclAST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> decl;

  void Dump(int option) const override {
    decl->Dump(option);
  }
};

class ConstDeclAST : public BaseAST {
 public:
  std::string bType;
  std::unique_ptr<BaseAST> constDef;
  std::unique_ptr<std::vector<BaseAST*>> constDefs;

  void Dump(int option) const override {
    constDef->Dump(option);
    std::vector<BaseAST*>::const_iterator i;
    for(i = (*constDefs).begin(); i != (*constDefs).end(); i++){
      (*i)->Dump(option);
    }
  }
};

class ConstDefAST : public BaseAST {
 public:
  std::string ident;
  std::unique_ptr<BaseAST> constInitVal;

  void Dump(int option) const override {
    constInitVal->Dump(CALCULATE_VALUE);
    tagUnion val;
    val.tag = CONSTANT;
    val.value.val = cal_num;
    (*(symbol_tables.back()))[ident] = val;
  }
};

class ConstInitValAST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> constExp;

  void Dump(int option) const override {
    constExp->Dump(option);
  }
};

class VarDeclAST : public BaseAST {
 public:
  std::string bType;
  std::unique_ptr<BaseAST> varDef;
  std::unique_ptr<std::vector<BaseAST*>> varDefs;

  void Dump(int option) const override {
    varDef->Dump(option);
    std::vector<BaseAST*>::const_iterator i;
    for(i = (*varDefs).begin(); i != (*varDefs).end(); i++){
      (*i)->Dump(option);
    }
  }
};

class VarDefAST : public BaseAST {
 public:
  std::string ident;
  std::unique_ptr<BaseAST> initVal;
  bool init;

  void Dump(int option) const override {
    std::string var_name = "@" + ident + "_" + std::to_string(var_idx);
    var_idx++;

    tagUnion val;
    val.tag = VARIABLE;
    val.value.var = new std::string(var_name);
    (*(symbol_tables.back()))[ident] = val;

    if (option == GLOBAL){
      std::cout << "global " << var_name << " = alloc i32, ";
      if (init){
        initVal->Dump(CALCULATE_VALUE);
        std::cout << cal_num << std::endl << std::endl;
      } else {
        std::cout << "zeroinit" << std::endl << std::endl; 
      }
    } else {
      std::cout << "  " << var_name << " = alloc i32" << std::endl;
      alloc++;
      if (init){
        int tmp = temp;
        initVal->Dump(DEFAULT);
        if (tmp != temp){
          std::cout << "  store %" << temp - 1 << ", " << var_name << std::endl;
        } else {
          std::cout << "  store ";
          initVal->Dump(PRINT_VALUE);
          std::cout << ", " << var_name << std::endl;
        }
      }
    }
  }
};

class VarArrayDefAST : public BaseAST {
 public:
  std::string ident;
  std::unique_ptr<BaseAST> constExp;
  std::unique_ptr<std::vector<BaseAST*>> constExps;
  std::unique_ptr<BaseAST> initVal;
  bool init;

  void Dump(int option) const override {
    std::string var_name = std::to_string((*constExps).size() + 1) + "@" + ident + "_" + std::to_string(var_idx);
    var_idx++;

    tagUnion val;
    val.tag = ARRAY;
    val.value.var = new std::string(var_name);
    (*(symbol_tables.back()))[ident] = val;

    if (option == GLOBAL){
      std::cout << "global ";
    } else {
      std::cout << "  ";
    }
    std::cout << var_name.substr(1) << " = alloc ";
    int exp_cnt = (*constExps).size() + 1;
    for (int i = 0; i < exp_cnt; i++){
      std::cout << "[";
    }
    std::cout << "i32";
    arrayParams.clear();
    curArrayParams.clear();
    int size = 1;
    std::vector<BaseAST*>::const_reverse_iterator j;
    for(j = (*constExps).rbegin(); j != (*constExps).rend(); j++){
      (*j)->Dump(CALCULATE_VALUE);
      std::cout << ", " << cal_num << "]";
      arrayParams.push_back(cal_num);
      curArrayParams.push_back(0);
      size *= cal_num;
    }
    constExp->Dump(CALCULATE_VALUE);
    std::cout << ", " << cal_num << "]";
    arrayParams.push_back(cal_num);
    curArrayParams.push_back(0);
    size *= cal_num;
    if (option == GLOBAL){
      std::cout << ", ";
      if (init){
        arrayContents.clear();
        arrayInitCnt = 0;
        initVal->Dump(GLOBAL);
        int cnt;
        for(cnt = 0; cnt < size; cnt++){
          int cnt2 = 1;
          bool parentheses = false;
          std::vector<int>::const_iterator k;
          for (k = arrayParams.begin(); k != arrayParams.end(); k++){
            cnt2 *= (*k);
            if (cnt % cnt2 == 0){
              std::cout << "{";
              parentheses = true;
            } else {
              break;
            }
          }
          if (!parentheses){
            std::cout << ", ";
          }
          std::cout << arrayContents[cnt];
          cnt2 = 1; parentheses = false;
          for (k = arrayParams.begin(); k != arrayParams.end(); k++){
            cnt2 *= (*k);
            if ((cnt + 1) % cnt2 == 0){
              std::cout << "}";
              parentheses = true;
            } else {
              if (parentheses){
                if (size != (cnt + 1)){
                  std::cout << ", ";
                }
              }
              break;
            }
          }
        }
        std::cout << std::endl << std::endl;
      } else {
        std::cout << "zeroinit" << std::endl << std::endl;
      }
    } else {
      std::cout << std::endl;
      alloc += size;
      curIdent = var_name.substr(1);
      arrayParams2 = arrayParams;
      arrayInitCnt = 0;
      if (init){
        initVal->Dump(DEFAULT);
      } else {
        for (int k = 0; k < size; k++){
          storeArray(PRINT_VALUE, 0);
        }
      }
    }
  }
};

class InitValAST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> exp;

  void Dump(int option) const override {
    exp->Dump(option);
  }
};

class InitArrayValAST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> initVal;
  std::unique_ptr<std::vector<BaseAST*>> initVals;
  bool init;

  void Dump(int option) const override {
    if (option == TYPE_CHECK){
      isExp = false;
      return;
    }
    int cnt = 1;
    std::vector<int>::const_iterator i;
    for(i = arrayParams.begin(); i != arrayParams.end(); i++){
      cnt *= (*i);
    }
    int tmp = arrayParams.back();
    arrayParams.pop_back();
    if (init){
      initVal->Dump(TYPE_CHECK);
      if (isExp){
        if (option == GLOBAL){
          initVal->Dump(CALCULATE_VALUE);
          arrayContents.push_back(cal_num);
        } else {
          int tmp = temp;
          initVal->Dump(DEFAULT);
          if (tmp != temp){
            storeArray(DEFAULT, DEFAULT);
          } else {
            initVal->Dump(CALCULATE_VALUE);
            storeArray(PRINT_VALUE, cal_num);
          }
        }
        arrayInitCnt++;
      } else {
        int cnt_tmp = 1;
        std::vector<int> tmp_vec;
        std::vector<int>::const_iterator j;
        for(j = arrayParams.begin(); j != arrayParams.end(); j++){
          cnt_tmp *= (*j);
          if (arrayInitCnt % cnt_tmp){
            break;
          } else {
            tmp_vec.push_back((*j));
          }
        }
        tmp_vec.swap(arrayParams);
        initVal->Dump(option);
        tmp_vec.swap(arrayParams);
      }
      std::vector<BaseAST*>::const_iterator k;
      for(k = (*initVals).begin(); k != (*initVals).end(); k++){
        (*k)->Dump(TYPE_CHECK);
        if (isExp){
          if (option == GLOBAL){
            (*k)->Dump(CALCULATE_VALUE);
            arrayContents.push_back(cal_num);
          } else {
            int tmp = temp;
            (*k)->Dump(DEFAULT);
            if (tmp != temp){
              storeArray(DEFAULT, DEFAULT);
            } else {
              (*k)->Dump(CALCULATE_VALUE);
              storeArray(PRINT_VALUE, cal_num);
            }
          }
          arrayInitCnt++;
        } else {
          int cnt_tmp = 1;
          std::vector<int> tmp_vec;
          std::vector<int>::const_iterator l;
          for(l = arrayParams.begin(); l != arrayParams.end(); l++){
            cnt_tmp *= (*l);
            if (arrayInitCnt % cnt_tmp){
              break;
            } else {
              tmp_vec.push_back((*l));
            }
          }
          tmp_vec.swap(arrayParams);
          (*k)->Dump(option);
          tmp_vec.swap(arrayParams);
        }
      }
      if (arrayInitCnt % cnt){
        int n = cnt - (arrayInitCnt % cnt);
        for (int i = 0; i < n; i++){
          if (option == GLOBAL){
            arrayContents.push_back(0);
          } else {
            storeArray(PRINT_VALUE, 0);
          }
        }
        arrayInitCnt += n;
      }
      
    } else {
      for (int i = 0; i < cnt; i++){
        if (option == GLOBAL){
          arrayContents.push_back(0);
        } else {
          storeArray(PRINT_VALUE, 0);
        }
      }
      arrayInitCnt += cnt;
    }
    arrayParams.push_back(tmp);
  }
};

class BlockAST : public BaseAST {
 public:
  std::unique_ptr<std::vector<BaseAST*>> blockItems;

  void Dump(int option) const override {
    std::map<std::string, tagUnion>* symbol_table = new std::map<std::string, tagUnion>;
    symbol_tables.push_back(symbol_table);
    std::vector<BaseAST*>::const_iterator i;
    for(i = (*blockItems).begin(); i != (*blockItems).end(); i++){
      (*i)->Dump(option);
    }
    symbol_tables.pop_back();
  }
};

class Stmt1AST : public BaseAST {
 public:
  std::string lVal;
  std::unique_ptr<BaseAST> exp;

  void Dump(int option) const override {
    int tmp = temp;
    exp->Dump(DEFAULT);

    std::string var_name;
    std::vector<std::map<std::string, tagUnion>*>::const_reverse_iterator i;
    for(i = symbol_tables.rbegin(); i != symbol_tables.rend(); i++){
      if ((*(*i)).find(lVal) != (*(*i)).end()) {
        if ((*(*i))[lVal].tag != VARIABLE){
          continue;
        }
        var_name = *((*(*i))[lVal].value.var);
        break;
      }
    }

    if (tmp != temp){
      std::cout << "  store %" << temp - 1 << ", " << var_name << std::endl;
    } else {
      std::cout << "  store ";
      exp->Dump(PRINT_VALUE);
      std::cout << ", " << var_name << std::endl;
    }
  }
};

class Stmt2AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> lVal;
  std::unique_ptr<BaseAST> exp;

  void Dump(int option) const override {
    int tmp = temp;
    exp->Dump(DEFAULT);
    if (tmp != temp){
      tmp = temp;
      lVal->Dump(STORE);
      std::cout << "  store %" << tmp - 1 << ", %" << temp - 1 << std::endl;
    } else {
      lVal->Dump(STORE);
      std::cout << "  store ";
      exp->Dump(PRINT_VALUE);
      std::cout << ", %" << temp - 1 << std::endl;
    }
  }
};

class Stmt3AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> exp;
  bool isExp;

  void Dump(int option) const override {
    if (isExp){
      exp->Dump(option);
    }
  }
};

class Stmt4AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> block;

  void Dump(int option) const override {
    block->Dump(option);
  }
};

class Stmt5AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> exp;
  bool isExp;

  void Dump(int option) const override {
    if (isExp){
      int tmp = temp;
      exp->Dump(DEFAULT);
      std::cout << "  ret ";
      if (tmp == temp){
        exp->Dump(PRINT_VALUE);
      } else {
        std::cout << "%" << temp - 1;
      }
      std::cout << std::endl;
    } else {
      std::cout << "  ret" << std::endl;
    }
    if (!std::cout.fail()){
      std::cout.setstate(std::ios_base::failbit);
    }
  }
};

class Stmt6AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> exp;
  std::unique_ptr<BaseAST> stmt;

  void Dump(int option) const override {
    int tmp_if_cnt = if_cnt; if_cnt++;
    int tmp_cur_if_cnt = cur_if_cnt; cur_if_cnt = tmp_if_cnt;
    std::cout << "  jump %while_entry" << tmp_if_cnt << std::endl;
    if (std::cout.fail()){
      std::cout.clear();
    }
    std::cout << std::endl << "%while_entry" << tmp_if_cnt << ":" << std::endl;
    int tmp = temp;
    exp->Dump(option);
    if (tmp != temp){
      std::cout << "  br %" << temp - 1 << ", %then" << tmp_if_cnt << ", %end" << tmp_if_cnt << std::endl;
    } else {
      std::cout << "  br ";
      exp->Dump(PRINT_VALUE);
      std::cout << ", %then" << tmp_if_cnt << ", %end" << tmp_if_cnt << std::endl;
    }
    if (std::cout.fail()){
      std::cout.clear();
    }
    std::cout << std::endl << "%then" << tmp_if_cnt << ":" << std::endl;
    stmt->Dump(option);
    std::cout << "  jump %while_entry" << tmp_if_cnt << std::endl;
    if (std::cout.fail()){
      std::cout.clear();
    }
    std::cout << std::endl << "%end" << tmp_if_cnt << ":" << std::endl;
    cur_if_cnt = tmp_cur_if_cnt;
  }
};

class Stmt7AST : public BaseAST {
 public:
  bool isBreak;

  void Dump(int option) const override {
    if (isBreak){
      std::cout << "  jump %end" << cur_if_cnt << std::endl;
      if (!std::cout.fail()){
        std::cout.setstate(std::ios_base::failbit);
      }
    } else {
      std::cout << "  jump %while_entry" << cur_if_cnt << std::endl;
      if (!std::cout.fail()){
        std::cout.setstate(std::ios_base::failbit);
      }
    }
  }
};

class IfElseAST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> exp;
  std::unique_ptr<BaseAST> ifStmt;
  std::unique_ptr<BaseAST> elseStmt;
  bool isElse;

  void Dump(int option) const override {
    int tmp = temp, tmp_if_cnt = if_cnt;
    if_cnt++;
    exp->Dump(option);
    if (tmp != temp){
      std::cout << "  br %" << temp - 1;
    } else {
      std::cout << "  br ";
      exp->Dump(PRINT_VALUE);
    }
    std::cout << ", %then" << tmp_if_cnt << ", %";
    if (isElse){
      std::cout << "else" << tmp_if_cnt << std::endl;
    } else {
      std::cout << "end" << tmp_if_cnt << std::endl;
    }

    if (std::cout.fail()){
      std::cout.clear();
    }
    std::cout << std::endl << "%then" << tmp_if_cnt << ":" << std::endl;
    ifStmt->Dump(option);
    std::cout << "  jump %end" << tmp_if_cnt << std::endl;

    if (std::cout.fail()){
      std::cout.clear();
    }
    if (isElse){
      std::cout << std::endl << "%else" << tmp_if_cnt << ":" << std::endl;
      elseStmt->Dump(DEFAULT);
      std::cout << "  jump %end" << tmp_if_cnt << std::endl;
      if (std::cout.fail()){
        std::cout.clear();
      }
    }
    std::cout << std::endl << "%end" << tmp_if_cnt << ":" << std::endl;
  }
};

class LArrayValAST : public BaseAST {
 public:
  std::string ident;
  std::unique_ptr<BaseAST> exp;
  std::unique_ptr<std::vector<BaseAST*>> exps;

  void Dump(int option) const override {
    //int idx = 0;
    std::string var_name;
    std::vector<std::map<std::string, tagUnion>*>::const_reverse_iterator i;
    for(i = symbol_tables.rbegin(); i != symbol_tables.rend(); i++){
      if ((*(*i)).find(ident) != (*(*i)).end()) {
        int tag = (*(*i))[ident].tag;
        if (tag != ARRAY){
          continue;
        }
        var_name = *((*(*i))[ident].value.var);
        break;
      }
    }
    bool isElem = (stoi(var_name.substr(0, 1)) == ((*exps).size() + 1)), isParam = (var_name[1] == '%');
    int tmp = temp, size = (*exps).size() + 1;
    exp->Dump(DEFAULT);
    if (isParam){
      std::cout << "  %ptr" << ptridx << " = load " << var_name.substr(1) << std::endl;
      ptridx++;
      if (size - 1){
        std::cout << "  %ptr" << ptridx << " = getptr %ptr" << ptridx - 1 << ", ";
        ptridx++;
      } else {
        std::cout << "  %" << temp << " = getptr %ptr" << ptridx - 1 << ", ";
      }
    } else {
      if (size - 1){
        std::cout << "  %ptr" << ptridx << " = getelemptr " << var_name.substr(1) << ", ";
        ptridx++;
      } else {
        std::cout << "  %" << temp << " = getelemptr " << var_name.substr(1) << ", ";
      }
    }
    size--;
    if (tmp != temp){
      std::cout << "%" << temp - 1 << std::endl;
    } else {
      exp->Dump(PRINT_VALUE);
      std::cout << std::endl;
    }
    std::vector<BaseAST*>::const_iterator j;
    for(j = (*exps).begin(); j != (*exps).end(); j++){
      tmp = temp;
      (*j)->Dump(DEFAULT);
      if (size - 1){
        std::cout << "  %ptr" << ptridx << " = getelemptr %ptr" << ptridx - 1 << ", ";
        ptridx++;
      } else {
        std::cout << "  %" << temp << " = getelemptr %ptr" << ptridx - 1 << ", ";
      }
      size--;
      if (tmp != temp){
        std::cout << "%" << temp - 1 << std::endl;
      } else {
        (*j)->Dump(PRINT_VALUE);
        std::cout << std::endl;
      }
    }
    temp++;
    if (option == STORE){
      return;
    }
    if (isElem){
      std::cout << "  %" << temp << " = load %" << temp - 1 << std::endl;
      temp++;
      return;
    }
    std::cout << "  %" << temp << " = getelemptr %" << temp - 1 << ", 0" << std::endl;
    temp++;
  }
};

class ExpAST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> exp;

  void Dump(int option) const override {
    if (option == TYPE_CHECK){
      isExp = true;
      return;
    }
    exp->Dump(option);
  }
};

class PrimaryExp1AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> exp;

  void Dump(int option) const override {
    exp->Dump(option);
  }
};

class PrimaryExp2AST : public BaseAST {
 public:
  int number;

  void Dump(int option) const override {
    if (option == PRINT_VALUE){
      std::cout << number;
    } else if (option == CALCULATE_VALUE){
      cal_num = number;
    }
  }
};

class PrimaryExp3AST : public BaseAST {
 public:
  std::string lVal;

  void Dump(int option) const override {
    tagUnion val;
    std::vector<std::map<std::string, tagUnion>*>::const_reverse_iterator i;
    for(i = symbol_tables.rbegin(); i != symbol_tables.rend(); i++){
      if ((*(*i)).find(lVal) != (*(*i)).end()) {
        val = (*(*i))[lVal];
        break;
      }
    }

    if (val.tag == VARIABLE){
      std::cout << "  %" << temp << " = load " << *(val.value.var) << std::endl;
      temp++;
      return;
    }
    if (val.tag == ARRAY){
      if (!(*(val.value.var)).substr(1, 1).compare("%")){
        std::cout << "  %" << temp << " = load " << (*(val.value.var)).substr(1) << std::endl;
      } else {
        std::cout << "  %" << temp << " = getelemptr " << (*(val.value.var)).substr(1) << ", 0" << std::endl;
      }
      temp++;
      return;
    }
    if (option == PRINT_VALUE){
      if (val.tag == CONSTANT){
        std::cout << val.value.val;
      }
    } else if (option == CALCULATE_VALUE){
      if (val.tag == CONSTANT){
        cal_num = val.value.val;
      } else if (val.tag == VARIABLE){
        
      }
    }
  }
};

class PrimaryExp4AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> lVal;

  void Dump(int option) const override {
    lVal->Dump(DEFAULT);
  }
};

class UnaryExp1AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> primaryExp;
  //int value;

  void Dump(int option) const override {
    primaryExp->Dump(option);
  }
};

class UnaryExp2AST : public BaseAST {
 public:
  std::string ident;
  std::unique_ptr<BaseAST> exp;
  std::unique_ptr<std::vector<BaseAST*>> exps;
  bool params;

  void Dump(int option) const override {
    std::string funcType;
    std::vector<std::map<std::string, tagUnion>*>::const_reverse_iterator i;
    for(i = symbol_tables.rbegin(); i != symbol_tables.rend(); i++){
      if ((*(*i)).find(ident) != (*(*i)).end()) {
        if ((*(*i))[ident].tag != FUNCTION){
          continue;
        }
        funcType = *((*(*i))[ident].value.var);
        break;
      }
    }
    if (params){
      std::vector<int> params_vec;
      int tmp = temp;
      exp->Dump(DEFAULT);
      if (tmp != temp){
        params_vec.push_back(temp - 1);
      } else {
        params_vec.push_back(-1);
      }
      std::vector<BaseAST*>::const_iterator i;
      for(i = (*exps).begin(); i != (*exps).end(); i++){
        tmp = temp;
        (*i)->Dump(DEFAULT);
        if (tmp != temp){
          params_vec.push_back(temp - 1);
        } else {
          params_vec.push_back(-1);
        }
      }
      if (params_vec.size() > maxParam){
        maxParam = params_vec.size();
      }
      if (!funcType.compare("int")){
        std::cout << "  %" << temp << " = call @" << ident << "(";
        temp++;
      } else {
        std::cout << "  call @" << ident << "(";
      }
      tmp = 0;
      if (params_vec[tmp] >= 0){
        std::cout << "%" << params_vec[tmp];
      } else {
        exp->Dump(PRINT_VALUE);
      }
      tmp++;
      for(i = (*exps).begin(); i != (*exps).end(); i++){
        if (params_vec[tmp] >= 0){
          std::cout << ", %" << params_vec[tmp];
        } else {
          std::cout << ", ";
          (*i)->Dump(PRINT_VALUE);
        }
        tmp++;
      }
      std::cout << ")" << std::endl;
    } else {
      if (!funcType.compare("int")){
        std::cout << "  %" << temp << " = call @" << ident << "()" << std::endl;
        temp++;
      } else {
        std::cout << "  call @" << ident << "()" << std::endl;
      }
    }
    callInst = true;
  }
};

class UnaryExp3AST : public BaseAST {
 public:
  std::string unaryOp;
  std::unique_ptr<BaseAST> unaryExp;

  void Dump(int option) const override {
    if (option == CALCULATE_VALUE){
      unaryExp->Dump(CALCULATE_VALUE);
      if (!unaryOp.compare("!")){
        cal_num = !cal_num;
      } else if (!unaryOp.compare("-")){
        cal_num = -cal_num;
      }
      return;
    }
    int tmp = temp;
    unaryExp->Dump(DEFAULT);
    if (!unaryOp.compare("!")){
      std::cout << "  %" << temp << " = eq ";
      if (tmp != temp){
        std::cout << "%" << temp - 1;
      } else {
        unaryExp->Dump(PRINT_VALUE);
      }
      std::cout << ", 0" << std::endl;
      temp++;
    } else if (!unaryOp.compare("-")){
      std::cout << "  %" << temp << " = sub 0, ";
      if (tmp != temp){
        std::cout << "%" << temp - 1 << std::endl;
      } else {
        unaryExp->Dump(PRINT_VALUE);
        std::cout << std:: endl;
      }
      temp++;
    } else {
      if (tmp == temp){
        unaryExp->Dump(option);
      }
    }
  }
};

class MulExp1AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> unaryExp;

  void Dump(int option) const override {
    unaryExp->Dump(option);
  }
};

class MulExp2AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> mulExp;
  std::string op;
  std::unique_ptr<BaseAST> unaryExp;

  void Dump(int option) const override {
    if (option == CALCULATE_VALUE){
      mulExp->Dump(CALCULATE_VALUE);
      int tmp = cal_num;
      unaryExp->Dump(CALCULATE_VALUE);
      if (!op.compare("*")){
        cal_num = tmp * cal_num;
      } else if (!op.compare("/")){
        cal_num = tmp / cal_num;
      } else {
        cal_num = tmp % cal_num;
      }
      return;
    }
    std::string instr;
    if (!op.compare("*")){
      instr = "mul";
    } else if (!op.compare("/")){
      instr = "div";
    } else {
      instr = "mod";
    }
    int tmp = temp;
    mulExp->Dump(DEFAULT);
    if (tmp != temp){
      tmp = temp;
      unaryExp->Dump(DEFAULT);
      if (tmp != temp){
        std::cout << "  %" << temp << " = " << instr << " %" << tmp - 1 << ", %" << temp - 1 << std::endl;
      } else {
        std::cout << "  %" << temp << " = " << instr << " %" << tmp - 1 << ", ";
        unaryExp->Dump(PRINT_VALUE);
        std::cout << std::endl;
      }
    } else {
      unaryExp->Dump(DEFAULT);
      if (tmp != temp){
        std::cout << "  %" << temp << " = " << instr << " ";
        mulExp->Dump(PRINT_VALUE);
        std::cout << ", %" << temp - 1 << std::endl;
      } else {
        std::cout << "  %" << temp << " = " << instr << " ";
        mulExp->Dump(PRINT_VALUE);
        std::cout  << ", ";
        unaryExp->Dump(PRINT_VALUE);
        std::cout << std::endl;
      }
    }
    temp++;
  }
};

class AddExp1AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> mulExp;

  void Dump(int option) const override {
    mulExp->Dump(option);
  }
};

class AddExp2AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> addExp;
  std::string op;
  std::unique_ptr<BaseAST> mulExp;

  void Dump(int option) const override {
    if (option == CALCULATE_VALUE){
      addExp->Dump(CALCULATE_VALUE);
      int tmp = cal_num;
      mulExp->Dump(CALCULATE_VALUE);
      if (!op.compare("+")){
        cal_num = tmp + cal_num;
      } else {
        cal_num = tmp - cal_num;
      }
      return;
    }
    std::string instr;
    if (!op.compare("+")){
      instr = "add";
    } else {
      instr = "sub";
    }
    int tmp = temp;
    addExp->Dump(DEFAULT);
    if (tmp != temp){
      tmp = temp;
      mulExp->Dump(DEFAULT);
      if (tmp != temp){
        std::cout << "  %" << temp << " = " << instr << " %" << tmp - 1 << ", %" << temp - 1 << std::endl;
      } else {
        std::cout << "  %" << temp << " = " << instr << " %" << tmp - 1 << ", ";
        mulExp->Dump(PRINT_VALUE);
        std::cout << std::endl;
      }
    } else {
      mulExp->Dump(DEFAULT);
      if (tmp != temp){
        std::cout << "  %" << temp << " = " << instr << " ";
        addExp->Dump(PRINT_VALUE);
        std::cout << ", %" << temp - 1 << std::endl;
      } else {
        std::cout << "  %" << temp << " = " << instr << " ";
        addExp->Dump(PRINT_VALUE);
        std::cout  << ", ";
        mulExp->Dump(PRINT_VALUE);
        std::cout << std::endl;
      }
    }
    temp++;
  }
};

class RelExp1AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> addExp;

  void Dump(int option) const override {
    addExp->Dump(option);
  }
};

class RelExp2AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> relExp;
  std::string op;
  std::unique_ptr<BaseAST> addExp;

  void Dump(int option) const override {
    if (option == CALCULATE_VALUE){
      relExp->Dump(CALCULATE_VALUE);
      int tmp = cal_num;
      addExp->Dump(CALCULATE_VALUE);
      if (!op.compare("<")){
        cal_num = (tmp < cal_num);
      } else if (!op.compare(">")){
        cal_num = (tmp > cal_num);
      } else if (!op.compare("<=")){
        cal_num = (tmp <= cal_num);
      } else {
        cal_num = (tmp >= cal_num);
      }
      return;
    }
    std::string instr;
    if (!op.compare("<")){
      instr = "lt";
    } else if (!op.compare(">")){
      instr = "gt";
    } else if (!op.compare("<=")){
      instr = "le";
    } else {
      instr = "ge";
    }
    int tmp = temp;
    relExp->Dump(DEFAULT);
    if (tmp != temp){
      tmp = temp;
      addExp->Dump(DEFAULT);
      if (tmp != temp){
        std::cout << "  %" << temp << " = " << instr << " %" << tmp - 1 << ", %" << temp - 1 << std::endl;
      } else {
        std::cout << "  %" << temp << " = " << instr << " %" << tmp - 1 << ", ";
        addExp->Dump(PRINT_VALUE);
        std::cout << std::endl;
      }
    } else {
      addExp->Dump(DEFAULT);
      if (tmp != temp){
        std::cout << "  %" << temp << " = " << instr << " ";
        relExp->Dump(PRINT_VALUE);
        std::cout << ", %" << temp - 1 << std::endl;
      } else {
        std::cout << "  %" << temp << " = " << instr << " ";
        relExp->Dump(PRINT_VALUE);
        std::cout  << ", ";
        addExp->Dump(PRINT_VALUE);
        std::cout << std::endl;
      }
    }
    temp++;
  }
};

class EqExp1AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> relExp;

  void Dump(int option) const override {
    relExp->Dump(option);
  }
};

class EqExp2AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> eqExp;
  std::string op;
  std::unique_ptr<BaseAST> relExp;

  void Dump(int option) const override {
    if (option == CALCULATE_VALUE){
      eqExp->Dump(CALCULATE_VALUE);
      int tmp = cal_num;
      relExp->Dump(CALCULATE_VALUE);
      if (!op.compare("==")){
        cal_num = (tmp == cal_num);\
      } else {
        cal_num = (tmp != cal_num);
      }
      return;
    }
    std::string instr;
    if (!op.compare("==")){
      instr = "eq";
    } else {
      instr = "ne";
    }
    int tmp = temp;
    eqExp->Dump(DEFAULT);
    if (tmp != temp){
      tmp = temp;
      relExp->Dump(DEFAULT);
      if (tmp != temp){
        std::cout << "  %" << temp << " = " << instr << " %" << tmp - 1 << ", %" << temp - 1 << std::endl;
      } else {
        std::cout << "  %" << temp << " = " << instr << " %" << tmp - 1 << ", ";
        relExp->Dump(PRINT_VALUE);
        std::cout << std::endl;
      }
    } else {
      relExp->Dump(DEFAULT);
      if (tmp != temp){
        std::cout << "  %" << temp << " = " << instr << " ";
        eqExp->Dump(PRINT_VALUE);
        std::cout << ", %" << temp - 1 << std::endl;
      } else {
        std::cout << "  %" << temp << " = " << instr << " ";
        eqExp->Dump(PRINT_VALUE);
        std::cout  << ", ";
        relExp->Dump(PRINT_VALUE);
        std::cout << std::endl;
      }
    }
    temp++;
  }
};

class LAndExp1AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> eqExp;

  void Dump(int option) const override {
    eqExp->Dump(option);
  }
};

class LAndExp2AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> lAndExp;
  std::string op;
  std::unique_ptr<BaseAST> eqExp;

  void Dump(int option) const override {
    if (option == CALCULATE_VALUE){
      lAndExp->Dump(CALCULATE_VALUE);
      int tmp = cal_num;
      if (tmp == 0){
        cal_num = 0;
        return;
      }
      eqExp->Dump(CALCULATE_VALUE);
      cal_num = (tmp && cal_num);
      return;
    }
    int tmp_if_cnt = if_cnt;
    std::cout << "  @result" << tmp_if_cnt << " = alloc i32" << std::endl;
    if_cnt++;
    std::cout << "  store 0, @result" << tmp_if_cnt << std::endl;
    alloc++;
    int tmp = temp;
    lAndExp->Dump(DEFAULT);
    if (tmp != temp){
      tmp = temp;
      std::cout << "  br %" << tmp - 1 << ", %then" << tmp_if_cnt << ", %end" << tmp_if_cnt << std::endl;
      std::cout << std::endl << "%then" << tmp_if_cnt << ":" << std::endl;
      eqExp->Dump(DEFAULT);
      if (tmp != temp){
        std::cout << "  %" << temp << " = ne 0, %" << temp - 1 << std::endl;
      } else {
        std::cout << "  %" << temp << " = ne 0, ";
        eqExp->Dump(PRINT_VALUE);
        std::cout << std::endl;
      }
    } else {
      tmp = temp;
      std::cout << "  br ";
      lAndExp->Dump(PRINT_VALUE);
      std::cout << ", %then" << tmp_if_cnt << ", %end" << tmp_if_cnt << std::endl;
      std::cout << std::endl << "%then" << tmp_if_cnt << ":" << std::endl;
      eqExp->Dump(DEFAULT);
      if (tmp != temp){
        std::cout << "  %" << temp << " = ne 0, %" << temp - 1 << std::endl;
      } else {
        std::cout << "  %" << temp << " = ne 0, ";
        eqExp->Dump(PRINT_VALUE);
        std::cout << std::endl;
      }
    }
    std::cout << "  store %" << temp << ", @result" << tmp_if_cnt << std::endl;
    temp++;
    std::cout << "  jump %end" << tmp_if_cnt << std::endl;
    std::cout << std::endl << "%end" << tmp_if_cnt << ":" << std::endl;
    std::cout << "  %" << temp << " = load @result" << tmp_if_cnt << std::endl;
    temp++;
  }
};

class LOrExp1AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> lAndExp;

  void Dump(int option) const override {
    lAndExp->Dump(option);
  }
};

class LOrExp2AST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> lOrExp;
  std::string op;
  std::unique_ptr<BaseAST> lAndExp;

  void Dump(int option) const override {
    if (option == CALCULATE_VALUE){
      lOrExp->Dump(CALCULATE_VALUE);
      int tmp = cal_num;
      if (tmp != 0){
        cal_num = 1;
        return;
      }
      lAndExp->Dump(CALCULATE_VALUE);
      cal_num = (tmp || cal_num);
      return;
    }
    int tmp_if_cnt = if_cnt;
    std::cout << "  @result" << tmp_if_cnt << " = alloc i32" << std::endl;
    if_cnt++;
    std::cout << "  store 1, @result" << tmp_if_cnt << std::endl;
    alloc++;
    int tmp = temp;
    lOrExp->Dump(DEFAULT);
    if (tmp != temp){
      std::cout << "  %" << temp << " = eq 0, %" << temp - 1 << std::endl;
      temp++;
      tmp = temp;
      std::cout << "  br %" << tmp - 1 << ", %then" << tmp_if_cnt << ", %end" << tmp_if_cnt << std::endl;
      std::cout << std::endl << "%then" << tmp_if_cnt << ":" << std::endl;
      lAndExp->Dump(DEFAULT);
      if (tmp != temp){
        std::cout << "  %" << temp << " = ne 0, %" << temp - 1 << std::endl;
      } else {
        std::cout << "  %" << temp << " = ne 0, ";
        lAndExp->Dump(PRINT_VALUE);
        std::cout << std::endl;
      }
    } else {
      std::cout << "  %" << temp << " = eq 0, ";
      lOrExp->Dump(PRINT_VALUE);
      std::cout << std::endl;
      temp++;
      tmp = temp;
      std::cout << "  br %" << tmp - 1 << ", %then" << tmp_if_cnt << ", %end" << tmp_if_cnt << std::endl;
      std::cout << std::endl << "%then" << tmp_if_cnt << ":" << std::endl;
      lAndExp->Dump(DEFAULT);
      if (tmp != temp){
        std::cout << "  %" << temp << " = ne 0, %" << temp - 1 << std::endl;
      } else {
        std::cout << "  %" << temp << " = ne 0, ";
        lAndExp->Dump(PRINT_VALUE);
        std::cout << std::endl;
      }
    }
    std::cout << "  store %" << temp << ", @result" << tmp_if_cnt << std::endl;
    temp++;
    std::cout << "  jump %end" << tmp_if_cnt << std::endl;
    std::cout << std::endl << "%end" << tmp_if_cnt << ":" << std::endl;
    std::cout << "  %" << temp << " = load @result" << tmp_if_cnt << std::endl;
    temp++;
  }
};