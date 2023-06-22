#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <memory>
#include <cstring>
#include "../ast.h"
#include "koopa.h"

using namespace std;

extern FILE *yyin;
extern int yyparse(unique_ptr<BaseAST> &ast);

void Visit(const koopa_raw_program_t &program);
void Visit(const koopa_raw_slice_t &slice);
void Visit(const koopa_raw_function_t &func);
void Visit(const koopa_raw_basic_block_t &bb, bool only_print_name);
void Visit(const koopa_raw_value_t &value);
void Visit(const koopa_raw_return_t &ret, void* address);
void Visit(const koopa_raw_integer_t &integer, void* address);
void Visit(const koopa_raw_aggregate_t &aggregate, void* address);
void Visit(const koopa_raw_binary_t &binary, void* address);
void Visit(const koopa_raw_func_arg_ref_t &func_arg_ref, void* address);
void Visit(const koopa_raw_type_kind *base, void* address);
void Visit(const koopa_raw_global_alloc_t &global_alloc, const koopa_raw_type_kind *base, void* address);
void Visit(const koopa_raw_get_ptr_t &get_ptr, void* address);
void Visit(const koopa_raw_get_elem_ptr_t &get_elem_ptr, void* address);
void Visit(const koopa_raw_load_t &load, void* address);
void Visit(const koopa_raw_store_t &store, void* address);
void Visit(const koopa_raw_branch_t &branch, void* address);
void Visit(const koopa_raw_jump_t &jump, void* address);
void Visit(const koopa_raw_call_t &call, void* address);

string new_register(); 
string old_register();
string val_pos(string val, bool addr);
deque<int> arrayInfo(const koopa_raw_type_kind *base);

vector<map<string, tagUnion>*> symbol_tables;

int BaseAST::temp = 0;

int ptridx = 0;                                       /* number of pointers in IR */
int var_idx = 0;                                      /* to differentiate variables of same name */
int if_cnt = 0;                                       /* number of branches */
int cur_if_cnt = 0;                                   /* to record current branch number */
int cal_num;                                          /* for calculating constant value */
int arrayInitCnt;                                     /* number of array elements */
vector<int> curArrayParams;                           /* store current array parameter */
vector<int> arrayParams;                              /* array parameters for current use */
vector<int> arrayParams2;                             /* copy of arrayParams */
vector<int> arrayContents;                            /* array elements for current use */
string curIdent;                                      /* identifier name */
bool isExp;                                           /* is this expression? */
int maxParam = 0;                                     /* maxium number of function parameters to transfer */
vector<int> maxParams;                                /* store maxParam for each function */
bool callInst = false;                                /* is there any call instruction in current function? */
int alloc = 0;                                        /* size of memory allocation for variables*/
vector<int> alloc_byte;                               /* store byte size for each function for stack allocation */

int alloc_byte_idx = 0;                               /* to index alloc_byte vector */
map<void *, string> op2reg;                           /* koopa value address to corresponding stack address or register */
int registers = 0;                                    /* store values to different registers */
map<void*, deque<int>> allocArrayInfo;                /* store array or pointer variable information */
int stack_byte = 0;                                   /* available stack space */
int funcParamIdx = 0;                                 /* to record the number of function parameter */
bool needAddr = false;                                /* value must be address */
bool needLoad;                                        /* value must be loaded */
string retString;                                     /* return instruction for current function in objective code */



int main(int argc, const char *argv[]) {
  assert(argc == 5);
  auto mode = argv[1];
  auto input = argv[2];
  auto output = argv[4];

  yyin = fopen(input, "r");
  assert(yyin);

  unique_ptr<BaseAST> ast;
  auto ret = yyparse(ast);
  assert(!ret);
  
  ofstream out(output);
  streambuf *coutbuf = cout.rdbuf();
  cout.rdbuf(out.rdbuf());

  ast->Dump(DEFAULT);
  cout << endl;

  if (!string(mode).compare("-riscv")){
    ifstream t(output);
    stringstream buffer;
    buffer << t.rdbuf();
    remove(output);
    t.close();

    ofstream out(output);
    streambuf *coutbuf = cout.rdbuf();
    cout.rdbuf(out.rdbuf());

    koopa_program_t program;
    koopa_error_code_t ret_koopa = koopa_parse_from_string(buffer.str().c_str(), &program);
    assert(ret_koopa == KOOPA_EC_SUCCESS);
    koopa_raw_program_builder_t builder = koopa_new_raw_program_builder();
    koopa_raw_program_t raw = koopa_build_raw_program(builder, program);
    koopa_delete_program(program);

    Visit(raw);

    koopa_delete_raw_program_builder(builder);
  }
  cout.rdbuf(coutbuf);

  return 0;
}

void Visit(const koopa_raw_program_t &program) {
  Visit(program.values);
  Visit(program.funcs);
}

void Visit(const koopa_raw_slice_t &slice) {
  for (size_t i = 0; i < slice.len; ++i) {
    auto ptr = slice.buffer[i];
    switch (slice.kind) {
      case KOOPA_RSIK_FUNCTION:
        Visit(reinterpret_cast<koopa_raw_function_t>(ptr));
        break;
      case KOOPA_RSIK_BASIC_BLOCK:
        Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr), false);
        break;
      case KOOPA_RSIK_VALUE:
        Visit(reinterpret_cast<koopa_raw_value_t>(ptr));
        break;
      default:
        cout << slice.kind << endl;
        assert(false);
    }
  }
}

void Visit(const koopa_raw_function_t &func) {
  void* val_address = const_cast<void *>(reinterpret_cast<const void *>(func));
  if (op2reg.find(val_address) != op2reg.end()) {
    return;
  }
  int return_type = func->ty->data.function.ret->tag;
  string func_name = func->name + 1;
  op2reg[val_address] = to_string(return_type) + func_name;
  if (!func_name.compare("getint") || !func_name.compare("getch") || !func_name.compare("getarray")
   || !func_name.compare("putint") || !func_name.compare("putch") || !func_name.compare("putarray") 
   || !func_name.compare("starttime") || !func_name.compare("stoptime")){
    return;
  }
  cout << "  .text" << endl;
  cout << "  .global "; 
  cout << func_name << endl;
  cout << func_name << ':' << endl;
  int stackframe = alloc_byte[alloc_byte_idx];
  bool call = false;
  if (stackframe % 16){
    stackframe += 1;
    call = true;
  }
  if (stackframe){
    if (stackframe > 2047){
      cout << "  li    t0, -" << stackframe << endl;
      cout << "  add   sp, sp, t0" << endl;
    } else {
      cout << "  addi  sp, sp, -" << stackframe << endl;
    }
  }
  string ra = val_pos(to_string(stackframe - 4) + "(sp)", true);
  if (call){
    cout << "  sw    ra, " << ra << endl;
  }
  if (maxParams[alloc_byte_idx] > 0){
    stack_byte = maxParams[alloc_byte_idx];
  }
  retString = "";
  if (call){
    retString += ("  lw    ra, " + ra + "\n");
  }
  if (stackframe > 2047){
    retString += ("  li    t0, " + to_string(stackframe));
    retString += "\n  add   sp, sp, t0\n";
  } else if (stackframe > 0) {
    retString += ("  addi  sp, sp, " + to_string(stackframe) + "\n");
  }
  retString += "  ret\n";
  Visit(func->bbs);
  if (return_type == KOOPA_RTT_UNIT){
    cout << retString;
  }
  if (cout.fail()){
    cout.clear();
  }
  cout << endl;
  alloc_byte_idx++;
  stack_byte = 0;
}

void Visit(const koopa_raw_basic_block_t &bb, bool only_print_name) {
  if (only_print_name){
    cout << (bb->name + 1);
    return;
  } else {
    if (strcmp((bb->name + 1), "entry") != 0){
      cout << retString;
      if (cout.fail()){
        cout.clear();
      }
      cout << endl << (bb->name + 1) << ":" << endl;
    }
  }
  Visit(bb->insts);
}

void Visit(const koopa_raw_value_t &value) {
  if (value->kind.tag == KOOPA_RVT_GET_PTR || value->kind.tag == KOOPA_RVT_GET_ELEM_PTR){
    needLoad = true;
  }
  void* val_address = const_cast<void *>(reinterpret_cast<const void *>(value));
  if (op2reg.find(val_address) != op2reg.end()) {
    if (value->kind.tag == KOOPA_RVT_GLOBAL_ALLOC){
      cout << "  la    " << new_register() << ", " << (value->name) + 1 << endl;
      if (needAddr){
        op2reg[val_address] = old_register();
      } else {
        op2reg[val_address] = "0(" + old_register() + ")";
      }
    }
    string val_addr = val_pos(op2reg[val_address], true);
    if (funcParamIdx > 8) {
      cout << "  lw    t0, " << val_addr  << endl;
      cout << "  sw    t0, " << (funcParamIdx - 9) * 4 << "(sp)" << endl;
      funcParamIdx++;
    } else if (funcParamIdx > 0) {
      cout << "  lw    a" << funcParamIdx - 1 << ", " << val_addr << endl;
      funcParamIdx++;
    }
    return;
  }


  const auto &kind = value->kind;
  switch (kind.tag) {
    case KOOPA_RVT_RETURN:
      Visit(kind.data.ret, val_address);
      break;
    case KOOPA_RVT_ZERO_INIT:
      cout << "  .zero " << (-funcParamIdx) * 4 << endl;
      break;
    case KOOPA_RVT_AGGREGATE:
      Visit(kind.data.aggregate, val_address);
      break;
    case KOOPA_RVT_INTEGER:
      Visit(kind.data.integer, val_address);
      break;
    case KOOPA_RVT_BINARY:
      Visit(kind.data.binary, val_address);
      break;
    case KOOPA_RVT_FUNC_ARG_REF:
      Visit(kind.data.func_arg_ref, val_address);
      break;
    case KOOPA_RVT_ALLOC:
      Visit(value->ty->data.pointer.base, val_address);
      break;
    case KOOPA_RVT_GLOBAL_ALLOC:
      cout << "  .data" << endl << "  .globl " << (value->name) + 1 << endl;
      cout << (value->name) + 1 << ":" << endl;
      Visit(kind.data.global_alloc, value->ty->data.pointer.base, val_address);
      break;
    case KOOPA_RVT_GET_PTR:
      Visit(kind.data.get_ptr, val_address);
      break;
    case KOOPA_RVT_GET_ELEM_PTR:
      Visit(kind.data.get_elem_ptr, val_address);
      break;
    case KOOPA_RVT_LOAD:
      Visit(kind.data.load, val_address);
      break;
    case KOOPA_RVT_STORE:
      Visit(kind.data.store, val_address);
      break;
    case KOOPA_RVT_BRANCH:
      Visit(kind.data.branch, val_address);
      break;
    case KOOPA_RVT_JUMP:
      Visit(kind.data.jump, val_address);
      break;
    case KOOPA_RVT_CALL:
      Visit(kind.data.call, val_address);
      break;
    default:
      cout << kind.tag << endl;
  }
}

void Visit(const koopa_raw_return_t &ret, void* address) {
  if (ret.value == NULL){
    return;
  }
  koopa_raw_value_t ret_value = ret.value;
  Visit(ret_value);
  string ret_reg = op2reg[const_cast<void *>(reinterpret_cast<const void *>(ret_value))];
  ret_reg = val_pos(ret_reg, false);
  cout << "  mv    a0, " << ret_reg << endl;
  cout << retString;
  if (!cout.fail()){
    cout.setstate(ios_base::failbit);
  }
} 

void Visit(const koopa_raw_integer_t &integer, void* address) {
  int32_t int_val = integer.value;
  if (funcParamIdx == 0){
    if (int_val == 0){
      op2reg[address] = "x0";
    } else {
      cout << "  li    " << new_register() << ", " << int_val << endl;
      op2reg[address] = old_register();
    }
  } else if (funcParamIdx > 8) {
    cout << "  li    t0, " << int_val << endl;
    cout << "  sw    t0, " << (funcParamIdx - 9) * 4 << "(sp)" << endl;
    funcParamIdx++;
  } else if (funcParamIdx > 0){
    cout << "  li    a" << funcParamIdx - 1 << ", " << int_val << endl;
    funcParamIdx++;
  } else {
    cout << "  .word " << int_val << endl;
  }
}

void Visit(const koopa_raw_aggregate_t &aggregate, void* address) {
  funcParamIdx = -1;
  Visit(aggregate.elems);
  funcParamIdx = 0;
}

void Visit(const koopa_raw_binary_t &binary, void* address) {
  Visit(binary.lhs);
  Visit(binary.rhs);

  string lhs = op2reg[const_cast<void *>(reinterpret_cast<const void *>(binary.lhs))];
  string rhs = op2reg[const_cast<void *>(reinterpret_cast<const void *>(binary.rhs))];
  string resultReg;

  lhs = val_pos(lhs, false);
  rhs = val_pos(rhs, false);

  if (!lhs.compare("x0")){
    if (!rhs.compare("x0")){
      resultReg = new_register();
    } else {
      resultReg = rhs;
    }
  } else {
    resultReg = lhs;
  }

  uint32_t op = binary.op;
  if (op == KOOPA_RBO_NOT_EQ){
    cout << "  xor   " << resultReg << ", " << lhs << ", " << rhs << endl;
    cout << "  snez  " << resultReg << ", " << resultReg << endl;
  } else if (op == KOOPA_RBO_EQ){
    cout << "  xor   " << resultReg << ", " << lhs << ", " << rhs << endl;
    cout << "  seqz  " << resultReg << ", " << resultReg << endl;
  } else if (op == KOOPA_RBO_GT){
    cout << "  slt   " << resultReg << ", " << rhs << ", " << lhs << endl;
  } else if (op == KOOPA_RBO_LT){
    cout << "  slt   " << resultReg << ", " << lhs << ", " << rhs << endl;
  } else if (op == KOOPA_RBO_GE){
    cout << "  slt   " << resultReg << ", " << lhs << ", " << rhs << endl;
    cout << "  xori  " << resultReg << ", " << resultReg << ", 1" << endl;
  } else if (op == KOOPA_RBO_LE){
    cout << "  slt   " << resultReg << ", " << rhs << ", " << lhs << endl;
    cout << "  xori  " << resultReg << ", " << resultReg << ", 1" << endl;
  } else if (op == KOOPA_RBO_ADD){
    cout << "  add   " << resultReg << ", " << lhs << ", " << rhs << endl;
  } else if (op == KOOPA_RBO_SUB){
    cout << "  sub   " << resultReg << ", " << lhs << ", " << rhs << endl;
  } else if (op == KOOPA_RBO_MUL){
    cout << "  mul   " << resultReg << ", " << lhs << ", " << rhs << endl;
  } else if (op == KOOPA_RBO_DIV){
    cout << "  div   " << resultReg << ", " << lhs << ", " << rhs << endl;
  } else if (op == KOOPA_RBO_MOD){
    cout << "  rem   " << resultReg << ", " << lhs << ", " << rhs << endl;
  } else if (op == KOOPA_RBO_AND){
    string resultReg2;
    if (!rhs.compare("x0")) {
      resultReg2 = new_register();
    } else {
      resultReg2 = rhs;
    }
    if (!lhs.compare("x0") && rhs.compare("x0")){
      resultReg = new_register();
    }
    cout << "  snez  " << resultReg << ", " << lhs << endl;
    cout << "  snez  " << resultReg2 << ", " << rhs << endl;
    cout << "  and   " << resultReg2 << ", " << resultReg << ", " << resultReg2 << endl;
    resultReg = resultReg2;
  } else if (op == KOOPA_RBO_OR){
    cout << "  or    " << resultReg << ", " << lhs << ", " << rhs << endl;
    cout << "  snez  " << resultReg << ", " << resultReg << endl;
  }
  string stack_addr = to_string(stack_byte*4) + "(sp)";
  op2reg[address] = stack_addr;
  stack_byte++;
  stack_addr = val_pos(stack_addr, true);
  cout << "  sw    " << resultReg << ", " << stack_addr << endl;
}

void Visit(const koopa_raw_func_arg_ref_t &func_arg_ref, void* address){
  int idx = func_arg_ref.index;
  if (idx < 8){
    op2reg[address] = "a" + to_string(idx);
  }
  else {
    int stackframe = alloc_byte[alloc_byte_idx];
    if (stackframe % 16){
      stackframe += 1;
    }
    op2reg[address] = to_string((idx - 8) * 4 + stackframe) + "(sp)";
  }
}

void Visit(const koopa_raw_type_kind *base, void* address){
  int type = base->tag;
  if (type == KOOPA_RTT_INT32){
    op2reg[address] = to_string(stack_byte*4) + "(sp)";
    stack_byte++;
  } else if (type == KOOPA_RTT_POINTER){
    op2reg[address] = to_string(stack_byte*4) + "(sp)";
    stack_byte++;
    allocArrayInfo[address] = arrayInfo(base);
  } else {
    op2reg[address] = to_string(stack_byte*4) + "(sp)";
    int cnt = 1;
    deque<int> d = arrayInfo(base);
    deque<int>::const_iterator i;
    for (i = d.begin(); i != d.end(); i++){
      cnt *= (*i);
    }
    stack_byte += cnt;
    allocArrayInfo[address] = d;
  }
}

void Visit(const koopa_raw_global_alloc_t &global_alloc, const koopa_raw_type_kind *base, void* address) {
  funcParamIdx = -1;
  if (base->tag == KOOPA_RTT_ARRAY){
    int cnt = 1;
    deque<int> d = arrayInfo(base);
    deque<int>::const_iterator i;
    for (i = d.begin(); i != d.end(); i++){
      cnt *= (*i);
    }
    funcParamIdx *= cnt;
    allocArrayInfo[address] = d;
  }
  Visit(global_alloc.init);
  funcParamIdx = 0;
  cout << endl;
  op2reg[address] = "global";
}

void Visit(const koopa_raw_get_ptr_t &get_ptr, void* address){
  needAddr = true;
  Visit(get_ptr.src);
  string src = op2reg[const_cast<void *>(reinterpret_cast<const void *>(get_ptr.src))];
  if (src.find("(") != string::npos) {
    string offset = src.substr(0, src.find("("));
    if (stoi(offset) > 2047){
      cout << "  li    " << new_register() << ", " << offset << endl;
      string old_reg = old_register();
      cout << "  add   " << new_register() << ", sp, " << old_reg << endl;
      src = "0(" + old_register() + ")";
    }
  }
  cout << "  lw    " << new_register() << ", " << src << endl;
  src = old_register();
  
  
  Visit(get_ptr.index);
  string index = op2reg[const_cast<void *>(reinterpret_cast<const void *>(get_ptr.index))];
  if (index.find("(sp)") != string::npos) {
    index = val_pos(index, false);
  }

  deque<int> d = allocArrayInfo[const_cast<void *>(reinterpret_cast<const void *>(get_ptr.src))];
  
  int size = 1;
  for (int i = 0; i < d.size(); i++){
    size *= d[i];
  }
  allocArrayInfo[address] = d;
  cout << "  li    " << new_register() << ", " << size * 4 << endl;
  string unit = old_register();
  cout << "  mul   " << unit << ", " << unit << ", " << index << endl;
  cout << "  add   " << src << ", " << src << ", " << unit << endl;
  string stack_addr = to_string(stack_byte*4) + "(sp)";
  op2reg[address] = stack_addr;
  stack_addr = val_pos(stack_addr, true);
  cout << "  sw    " << src << ", " << stack_addr << endl;
  stack_byte++;
  needAddr = false;
}

void Visit(const koopa_raw_get_elem_ptr_t &get_elem_ptr, void* address) {
  needAddr = true;
  needLoad = false;
  Visit(get_elem_ptr.src);
  string src = op2reg[const_cast<void *>(reinterpret_cast<const void *>(get_elem_ptr.src))];
  if (src.find("(") != string::npos) {
    if (needLoad){
      src = val_pos(src, false);
    } else {
      string offset = src.substr(0, src.find("("));
      if (stoi(offset) > 2047){
        cout << "  li    " << new_register() << ", " << offset << endl;
        string old_reg = old_register();
        cout << "  add   " << new_register() << ", sp, " << old_reg << endl;
      } else {
        cout << "  addi  " << new_register() << ", sp, " << offset << endl;
      }
      src = old_register();
    }
  }
  
  Visit(get_elem_ptr.index);
  string index = op2reg[const_cast<void *>(reinterpret_cast<const void *>(get_elem_ptr.index))];
  if (index.find("(sp)") != string::npos) {
    index = val_pos(index, false);
  }

  deque<int> d = allocArrayInfo[const_cast<void *>(reinterpret_cast<const void *>(get_elem_ptr.src))];
  
  d.pop_front();
  int size = 1;
  for (int i = 0; i < d.size(); i++){
    size *= d[i];
  }
  
  allocArrayInfo[address] = d;
  cout << "  li    " << new_register() << ", " << size * 4 << endl;
  string unit = old_register();
  cout << "  mul   " << unit << ", " << unit << ", " << index << endl;
  cout << "  add   " << src << ", " << src << ", " << unit << endl;
  string stack_addr = to_string(stack_byte*4) + "(sp)";
  op2reg[address] = stack_addr;
  stack_addr = val_pos(stack_addr, true);
  cout << "  sw    " << src << ", " << stack_addr << endl;
  stack_byte++;
  needAddr = false;
}

void Visit(const koopa_raw_load_t &load, void* address) {
  needLoad = false;
  Visit(load.src);

  string src = op2reg[const_cast<void *>(reinterpret_cast<const void *>(load.src))];
  string stack_addr = to_string(stack_byte*4) + "(sp)";
  op2reg[address] = stack_addr;
  stack_byte++;

  if (allocArrayInfo.find(const_cast<void *>(reinterpret_cast<const void *>(load.src))) != allocArrayInfo.end()){
    deque<int> d = allocArrayInfo[const_cast<void *>(reinterpret_cast<const void *>(load.src))];
    if (d.size() && d[0] == -1){
      d.pop_front();
      allocArrayInfo[address] = d;
    }
  }

  src = val_pos(src, true);
  if (needLoad){
    cout << "  lw    " << new_register() << ", " << src << endl;
    src = "0(" + old_register() + ")";
  }
  stack_addr = val_pos(stack_addr, true);
  cout << "  lw    " << new_register() << ", " << src << endl;
  cout << "  sw    " << old_register() << ", " << stack_addr << endl;
}

void Visit(const koopa_raw_store_t &store, void* address) {
  Visit(store.value);
  needLoad = false;
  Visit(store.dest);
  
  string value = op2reg[const_cast<void *>(reinterpret_cast<const void *>(store.value))];
  string dest = op2reg[const_cast<void *>(reinterpret_cast<const void *>(store.dest))];

  value = val_pos(value, false);
  dest = val_pos(dest, true);
  if (needLoad){
    cout << "  lw    " << new_register() << ", " << dest << endl;
    dest = "0(" + old_register() + ")";
  }
  cout << "  sw    " << value << ", " << dest << endl;
}

void Visit(const koopa_raw_branch_t &branch, void* address) {
  Visit(branch.cond);
  string cond = op2reg[const_cast<void *>(reinterpret_cast<const void *>(branch.cond))];
  cond = val_pos(cond, false);
  cout << "  bnez  " << cond << ", ";
  Visit(branch.true_bb, true);
  cout << endl << "  j     ";
  Visit(branch.false_bb, true);
  cout << endl;
  if (!cout.fail()){
    cout.setstate(ios_base::failbit);
  }
}

void Visit(const koopa_raw_jump_t &jump, void* address) {
  cout << "  j     ";
  Visit(jump.target, true);
  cout << endl;
  if (!cout.fail()){
    cout.setstate(ios_base::failbit);
  }
}

void Visit(const koopa_raw_call_t &call, void* address) {
  Visit(call.callee);
  string callee_name = op2reg[const_cast<void *>(reinterpret_cast<const void *>(call.callee))];
  funcParamIdx = 1;
  Visit(call.args);
  cout << "  call  " << callee_name.substr(1) << endl;
  funcParamIdx = 0;
  if (stoi(callee_name.substr(0, 1)) == KOOPA_RTT_INT32){
    string val_addr = to_string(stack_byte*4) + "(sp)";
    op2reg[address] = val_addr;
    val_addr = val_pos(val_addr, true);
    cout << "  sw    a0, " << val_addr << endl;
    stack_byte++;
  }
}

string new_register(){
  string ret;
  ret = "t" + to_string(registers % 7);
  registers++;
  return ret;
}

string old_register(){
  string ret;
  ret = "t" + to_string((registers - 1) % 7);
  return ret;
}

string val_pos(string val, bool addr){
  if (val.find("(sp)") != string::npos) {
    int offset = stoi(val.substr(0, val.find("(")));
    if (offset > 2047){
      cout << "  li    " << new_register() << ", " << offset << endl;
      string old_reg = old_register();
      cout << "  add   " << new_register() << ", sp, " << old_reg << endl;
      old_reg = old_register();
      if (addr){
        return ("0(" + old_reg + ")");
      }
      cout << "  lw    " << new_register() << ", 0(" << old_reg << ")" << endl;
    } else {
      if (addr){
        return val;
      }
      cout << "  lw    " << new_register() << ", " << val << endl;
    }
    return old_register();
  } else {
    return val;
  }
}

deque<int> arrayInfo(const koopa_raw_type_kind *base){
  deque<int> d;
  while (1){
    if (base->tag == KOOPA_RTT_INT32){
      return d;
    } else if (base->tag == KOOPA_RTT_ARRAY){
      d.push_back(base->data.array.len);
      base = base->data.array.base;
    } else if (base->tag == KOOPA_RTT_POINTER){
      d.push_back(-1);
      base = base->data.pointer.base;
    }
  }
}

void storeArray(int option, int value){
  vector<int>::const_reverse_iterator i;
  bool first = true;
  for(i = curArrayParams.rbegin(); i != curArrayParams.rend(); i++){
    if (first){
      cout << "  %ptr" << ptridx << " = getelemptr " << curIdent << ", " << (*i) << endl;
      first = false;
    } else {
      cout << "  %ptr" << ptridx << " = getelemptr %ptr" << ptridx - 1 << ", " << (*i) << endl;
    }
    ptridx++;
  }
  if (option == PRINT_VALUE){
    cout << "  store " << value << ", %ptr" << ptridx - 1 << endl;
  } else {
    cout << "  store %" << BaseAST::temp - 1<< ", %ptr" << ptridx - 1 << endl;
  }
  int size = curArrayParams.size();
  for (int i = 0; i < size; i++){
    curArrayParams[i]++;
    if (curArrayParams[i] == arrayParams2[i]){
      curArrayParams[i] = 0;
    } else {
      break;
    }
  }
}