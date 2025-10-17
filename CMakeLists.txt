add_llvm_library( LLVMOurSinkPass MODULE BUILDTREE_ONLY
  OurSinkPass.cpp

  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
  opt
  )
