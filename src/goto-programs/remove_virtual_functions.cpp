/*******************************************************************\

Module: Remove Virtual Function (Method) Calls

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <util/prefix.h>

#include "class_hierarchy.h"
#include "remove_virtual_functions.h"

/*******************************************************************\

   Class: remove_virtual_functionst

 Purpose:

\*******************************************************************/

class remove_virtual_functionst
{
public:
  remove_virtual_functionst(
    const symbol_tablet &_symbol_table,
    const goto_functionst &goto_functions);

  void operator()(goto_functionst &goto_functions);

  bool remove_virtual_functions(goto_programt &goto_program);

protected:
  const namespacet ns;
  const symbol_tablet &symbol_table;
  
  class_hierarchyt class_hierarchy;

  void remove_virtual_function(
    goto_programt &goto_program,
    goto_programt::targett target);
    
  class functiont
  {
  public:
    symbol_exprt symbol_expr;
    irep_idt class_id;
  };

  typedef std::vector<functiont> functionst;
  void get_functions(const symbol_exprt &, functionst &);
  exprt get_method(const irep_idt &class_id, const symbol_exprt &);
  
  exprt build_class_identifier(const exprt &);
};

/*******************************************************************\

Function: remove_virtual_functionst::remove_virtual_functionst

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

remove_virtual_functionst::remove_virtual_functionst(
  const symbol_tablet &_symbol_table,
  const goto_functionst &goto_functions):
  ns(_symbol_table),
  symbol_table(_symbol_table)
{
  class_hierarchy(symbol_table);
}

/*******************************************************************\

Function: remove_virtual_functionst::build_class_identifier

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

exprt remove_virtual_functionst::build_class_identifier(
  const exprt &src)
{
  // the class identifier is in the root class
  exprt e=src;
  
  while(1)
  {
    const typet &type=ns.follow(e.type());
    assert(type.id()==ID_struct);
    
    const struct_typet &struct_type=to_struct_type(type);
    const struct_typet::componentst &components=struct_type.components();
    assert(!components.empty());
    
    member_exprt member_expr(
      e, components.front().get_name(), components.front().type());
    
    if(components.front().get_name()=="@class_identifier")
    {
      // found it
      return member_expr;
    }
    else
    {
      e=member_expr;
    }
  }
}

/*******************************************************************\

Function: remove_virtual_functionst::remove_virtual_function

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void remove_virtual_functionst::remove_virtual_function(
  goto_programt &goto_program,
  goto_programt::targett target)
{
  const code_function_callt &code=
    to_code_function_call(target->code);

  const exprt &function=code.function();
  assert(function.id()==ID_virtual_function);
  assert(function.operands().size()==1);
  assert(!code.arguments().empty());
  
  functionst functions;
  get_functions(to_symbol_expr(function.op0()), functions);
  
  if(functions.empty())
  {
    target->make_skip();
    return; // give up
  }

  // the final target is a skip
  goto_programt final_skip;

  goto_programt::targett t_final=final_skip.add_instruction();
  t_final->make_skip();
  
  // build the calls and gotos

  goto_programt new_code_calls;
  goto_programt new_code_gotos;

  for(functionst::const_iterator
      it=functions.begin();
      it!=functions.end();
      it++)
  {
    // call function
    goto_programt::targett t1=new_code_calls.add_instruction();
    t1->make_function_call(code);
    to_code_function_call(t1->code).function()=it->symbol_expr;
    
    // goto final
    goto_programt::targett t3=new_code_calls.add_instruction();
    t3->make_goto(t_final, true_exprt());

    exprt this_expr=code.arguments()[0];    
    if(this_expr.type().id()!=ID_pointer ||
       this_expr.type().id()!=ID_struct)
    {
      symbol_typet symbol_type(it->class_id);
      this_expr=typecast_exprt(this_expr, pointer_typet(symbol_type));
    }
    
    exprt deref=dereference_exprt(this_expr, this_expr.type().subtype());
    exprt c_id1=constant_exprt(it->class_id, string_typet());
    exprt c_id2=build_class_identifier(deref);
    
    goto_programt::targett t4=new_code_gotos.add_instruction();
    t4->make_goto(t1, equal_exprt(c_id1, c_id2));
  }

  goto_programt new_code;
  
  // patch them all together
  new_code.destructive_append(new_code_gotos);
  new_code.destructive_append(new_code_calls);
  new_code.destructive_append(final_skip);
  
  // set locations
  Forall_goto_program_instructions(it, new_code)
  {
    irep_idt property_class=it->source_location.get_property_class();
    irep_idt comment=it->source_location.get_comment();
    it->source_location=target->source_location;
    it->function=target->function;
    if(!property_class.empty()) it->source_location.set_property_class(property_class);
    if(!comment.empty()) it->source_location.set_comment(comment);
  }
  
  goto_programt::targett next_target=target;
  next_target++;
  
  goto_program.destructive_insert(next_target, new_code);
  
  // finally, kill original invocation
  target->make_skip();
}

/*******************************************************************\

Function: remove_virtual_functionst::get_functions

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

#include <iostream>

void remove_virtual_functionst::get_functions(
  const symbol_exprt &function,
  functionst &functions)
{
  irep_idt class_id=function.get(ID_C_class);
  assert(!class_id.empty());
  
  std::cout << "GETTING " << function.get_identifier() << " " << class_id << "\n"; 
  
  class_hierarchy.output(std::cout);
  
  // iterate over all children, transitively
  std::vector<irep_idt> children;
  class_hierarchy.get_children(class_id, children);
    std::cout << "CHILDREN: " << children.size() << "\n";

  for(const auto & child : children)
  {
    std::cout << "CHILD: " << child << "\n";
    exprt method=get_method(child, function);
    if(method.is_not_nil())
    {
      functiont function;
      function.class_id=child;
      function.symbol_expr=to_symbol_expr(method);
      functions.push_back(function);
    }
  }
  
  // Start from current class, go to parents until something
  // is found.
  irep_idt c=class_id;
  while(!c.empty())
  {
    exprt method=get_method(c, function);
    if(method.is_not_nil())
    {
      functiont function;
      function.class_id=c;
      function.symbol_expr=to_symbol_expr(method);
      functions.push_back(function);
      break; // abort
    }
    c=class_hierarchy.get_parent(c);
  }
}

/*******************************************************************\

Function: remove_virtual_functionst::get_method

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

exprt remove_virtual_functionst::get_method(
  const irep_idt &class_id,
  const symbol_exprt &virtual_function)
{
  irep_idt f_id=virtual_function.get_identifier();
  irep_idt c_id=virtual_function.get(ID_C_class);

  if(!has_prefix(id2string(f_id), id2string(c_id)))
    return nil_exprt();

  std::string suffix=
    std::string(id2string(f_id), c_id.size(), std::string::npos);
    
  irep_idt id=id2string(class_id)+suffix;
  
  const symbolt *symbol;
  if(ns.lookup(id, symbol))
    return nil_exprt();
  
  return symbol->symbol_expr();
}

/*******************************************************************\

Function: remove_virtual_functionst::remove_virtual_functions

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool remove_virtual_functionst::remove_virtual_functions(
  goto_programt &goto_program)
{
  bool did_something=false;

  Forall_goto_program_instructions(target, goto_program)
    if(target->is_function_call())
    {
      const code_function_callt &code=
        to_code_function_call(target->code);
        
      if(code.function().id()==ID_virtual_function)
      {
        remove_virtual_function(goto_program, target); 
        did_something=true;
      }
    }

  if(did_something)
  {
    //remove_skip(goto_program);
    goto_program.update();
  }

  return did_something;
}

/*******************************************************************\

Function: remove_virtual_functionst::operator()

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void remove_virtual_functionst::operator()(goto_functionst &functions)
{
  bool did_something=false;
  
  for(goto_functionst::function_mapt::iterator f_it=
      functions.function_map.begin();
      f_it!=functions.function_map.end();
      f_it++)
  {
    goto_programt &goto_program=f_it->second.body;

    if(remove_virtual_functions(goto_program))
      did_something=true;
  }

  if(did_something)
    functions.compute_location_numbers();
}

/*******************************************************************\

Function: remove_virtual_functions

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void remove_virtual_functions(
  const symbol_tablet &symbol_table,
  goto_functionst &goto_functions)
{
  remove_virtual_functionst
    rvf(symbol_table, goto_functions);

  rvf(goto_functions);
}

/*******************************************************************\

Function: remove_virtual_functions

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void remove_virtual_functions(goto_modelt &goto_model)
{
  remove_virtual_functions(
    goto_model.symbol_table, goto_model.goto_functions);
}