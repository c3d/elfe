#!/usr/bin/python

import lldb
import fblldbbase as fb

def lldbcommands():
  return [
    PrintTree(),
    PrintValue(),
    PrintTypes(),
    PrintScope(),
    PrintGlobalScope(),
    PrintContext(),
    PrintUnifications(),
  ]

class PrintTree(fb.FBCommand):
  def name(self):
    return 'tree'

  def description(self):
    return "Print the value of an XL parse tree"

  def args(self):
    return [
      fb.FBCommandArgument(arg='object', type='Tree *', help='Tree to print.')
    ]

  def run(self, arguments, options):
    lldb.debugger.HandleCommand('p debugp(%s)' % arguments[0])


class PrintValue(fb.FBCommand):
  def name(self):
    return 'value'

  def description(self):
    return "Print the value of an LLVM value"

  def args(self):
    return [
      fb.FBCommandArgument(arg='object', type='Value *', help='Value to print.')
    ]

  def run(self, arguments, options):
    lldb.debugger.HandleCommand('p debugv(%s)' % arguments[0])


class PrintTypes(fb.FBCommand):
  def name(self):
    return 'types'

  def description(self):
    return "Print the value of a types table"

  def args(self):
    return [
      fb.FBCommandArgument(arg='object', type='Types *', help='Value to print.')
    ]

  def run(self, arguments, options):
    lldb.debugger.HandleCommand('p debugt(%s)' % arguments[0])


class PrintUnifications(fb.FBCommand):
  def name(self):
    return 'unifications'

  def description(self):
    return "Print the unifications in a types table"

  def args(self):
    return [
      fb.FBCommandArgument(arg='object', type='Types *', help='Value to print.')
    ]

  def run(self, arguments, options):
    lldb.debugger.HandleCommand('p debugu(%s)' % arguments[0])


class PrintScope(fb.FBCommand):
  def name(self):
    return 'scope'

  def description(self):
    return "Print the current scope"

  def args(self):
    return [
      fb.FBCommandArgument(arg='object', type='Value *', help='Value to print.')
    ]

  def run(self, arguments, options):
    lldb.debugger.HandleCommand('p debugl(%s)' % arguments[0])


class PrintGlobalScope(fb.FBCommand):
  def name(self):
    return 'globalscope'

  def description(self):
    return "Print the current scope and all enclosing scopes"

  def args(self):
    return [
      fb.FBCommandArgument(arg='object', type='Value *', help='Value to print.')
    ]

  def run(self, arguments, options):
    lldb.debugger.HandleCommand('p debugg(%s)' % arguments[0])


class PrintContext(fb.FBCommand):
  def name(self):
    return 'context'

  def description(self):
    return "Print the current context"

  def args(self):
    return [
      fb.FBCommandArgument(arg='object', type='Value *', help='Value to print.')
    ]

  def run(self, arguments, options):
    lldb.debugger.HandleCommand('p debugc(%s)' % arguments[0])