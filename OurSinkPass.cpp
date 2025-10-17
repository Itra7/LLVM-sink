#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>
#include <iostream>
#include <unordered_set>

using namespace llvm;

namespace {
  struct OurSinkPass : public FunctionPass {
    static char ID;
    OurSinkPass() : FunctionPass(ID) {}
    // std::vector<BasicBlock*> FunctionBB;
    // std::unordered_map<BasicBlock*, std::vector<BasicBlock*>> Graph;
    // std::unordered_map<BasicBlock*, std::vector<Instruction*>> BBUses;
    // std::unordered_map<BasicBlock*, std::vector<std::pair<BasicBlock*, Instruction*>>> Uses;
    // std::unordered_set<BasicBlock*> Visited;

    bool Changed;
    std::vector<BasicBlock*> UsedInstruction;

    bool SafeToSink(Instruction *I){
      if (I->isTerminator() || isa<PHINode>(I) || isa<AllocaInst>(I) || isa<CallInst>(I)){
        return false;
      }
      
      return true;
    }

    // void dfs(BasicBlock *BB){
    //   if(Visited.find(BB) != Visited.end()){
    //     return;
    //   }
    //   for(Instruction &I : *BB){
    //     errs() << "Instruction name " << I.getOpcodeName() << "\n";
    //     if(!SafeToSink(&I)){
    //      continue;
    //     }
    //     // trazimo gde se ovo koristi u prethodnim BB
    //     for(auto &pair : BBUses){
    //       if(pair.first == BB){
    //         continue;
    //       }
    //       for(Instruction *IBB : pair.second){
    //         for(size_t i = 0; i < I.getNumOperands(); i++){
    //           if(I.getOperand(i) == IBB){
    //             // koristi se u ovom BB funckija IBB iz nmp kojeg BasicBlock-a
    //             errs() << "in this BB Instruction copy is  " << IBB->getOpcodeName() << "\n";
    //             Uses[BB].push_back({pair.first, IBB}); 
    //           }
    //         }
    //       }
    //     }
    //     // kad ti treba u kojem BB se koristi prodjes sve BBUses i nadjes
    //     // takodje ti trebaju i neki prethodnici.

    //     BBUses[BB].push_back(&I);
    //   }
    //   Visited.insert(BB);

    //   if(BranchI *BI = dyn_cast<BranchI>(BB->getTerminator())){
    //     Graph[BB].push_back(BI->getSuccessor(0));
    //     dfs(BI->getSuccessor(0));
    //     if(BI->isConditional()){
    //       Graph[BB].push_back(BI->getSuccessor(1));
    //       dfs(BI->getSuccessor(1));
    //     }
    //   }
    // }

    // void CreateGraph(Function *F){
    //   for(BasicBlock &BB : *F){
    //     dfs(&BB);
    //     FunctionBB.push_back(&BB);
    //   }
    // }

    // bool runOnFunction(Function &F) override {
    //   CreateGraph(&F);
      
    //   preskacemo prvi basic block
    //   for(size_t i = 1; i < FunctionBB.size(); i++){
    //     // Ako nema ponovljenih  
    //     if(Uses.find(FunctionBB[i]) == Uses.end()){
    //       continue;
    //     }

    //     std::vector<std::pair<BasicBlock*, Instruction*>> V = Uses[FunctionBB[i]];
    //     std::set<Instruction*> InstructionToSkip;
    //     std::set<Instruction*> Instruction;
    //     for(size_t j = 0; j < V.size(); j++){
    //       if(InstructionToSkip.find(V[j].second) != InstructionToSkip.end()){
    //           Instruction.insert(V[j].second);
    //       }else{
    //         InstructionToSkip.insert(V[j].second);
    //       }
    //     }
        
    //     for(Instruction *I : InstructionToSkip){
    //       errs() << "Skipping " << I->getOpcodeName() << "\n";
    //     }
    //     for(Instruction* I : Instruction){
    //       errs() << "processing " << I->getOpcodeName() << "\n";
    //     }
    //   return true;
    // }
    

    // Vraca niz BB-ova gde se ova Irukcija koristi
    void getUsedBlocks(Instruction *I){
      std::unordered_set<BasicBlock*> exists;

      // prolazimo kroz sva koriscenja 
      // opasnost: moze da koristi i u svom BB pa onda nema smisla sinkovati
      for(User *U : I->users()){
        if(Instruction *UI = dyn_cast<Instruction>(U)){
          // uzimamo BB u kojem se nalazi
          BasicBlock *BB = UI->getParent();
          bool usedInSet = exists.insert(BB).second;
          // ako postoji u setu znaci da se koristi
          if(usedInSet){
            UsedInstruction.push_back(BB);
          }
        }
      }
    }


    inline void reverseVector(std::vector<Instruction*> *InstructionToSink){
      size_t n = InstructionToSink->size();
      for(size_t i = 0; i < n/2; i++){
        std::swap((*InstructionToSink)[i], (*InstructionToSink)[n-i-1]);
      }
    }

    // vracamo prvu instrukciju koja koristi nasu instrukciju
    Instruction *findInsertionPoint(Instruction *I, BasicBlock *ToMoveBB) {
      std::vector<Instruction*> UIT;
      for (User *U : I->users()){
        if (auto *UserInst = dyn_cast<Instruction>(U)){
          if (UserInst->getParent() == ToMoveBB){
            UIT.push_back(UserInst);
          }
        }
      }
        
      if (UIT.empty()) return nullptr;
      
      // trazimo onu koja je prva
      Instruction *InsertPoint = UIT[0];
      for (Instruction *User : UIT) {
        if (User->comesBefore(InsertPoint)) {
          InsertPoint = User;
        }
      }
      return InsertPoint;
    }

    bool sinking(BasicBlock *BB){
      bool sinked = false;

      std::vector<Instruction*> InstructionToSink;
      for(Instruction &I : *BB){
        // ne mozemo sinkovati phi br i ostale.
        if(SafeToSink(&I)){
          InstructionToSink.push_back(&I);
        }
      }

      // GRESKA
      // moramo prvo unazad da iteriramo zbog zavisnosti Irukcija
      reverseVector(&InstructionToSink);
      for(Instruction *I : InstructionToSink){
        if(!SafeToSink(I)){
          continue;
        }

        // dobijamo sve blokove gde se ova instrukcija koristi
        UsedInstruction.clear();
        getUsedBlocks(I);

        // ako se nigde ne koristi
        if(UsedInstruction.empty()){
          continue;
        }


        // nema smisla sinkovati ako se vec koristi u parent blocku.
        bool usesInParentBB = false;
        for (BasicBlock *UseBB : UsedInstruction){
          if (UseBB == BB){
            usesInParentBB = true;
            break;
          }
        }

        if (usesInParentBB){
          continue;
        }

        // nema smisla raditi to ako imamo previse koriscenja.
        if(UsedInstruction.size() >= 3){
          continue;
        }

        if(UsedInstruction.size() == 1){
          BasicBlock *ToMoveBB = UsedInstruction[0];
          Instruction *InsertBefore = findInsertionPoint(I, ToMoveBB);
  
          if (!InsertBefore) {
            // ne bi trebalo nikad da se desi?
            InsertBefore = ToMoveBB->getTerminator();
          }

          I->moveBefore(InsertBefore);
          sinked = true;

        }else{
          size_t n = UsedInstruction.size();
          // Kopiramo na sva mesta osim zadnjeg
          for (size_t i = 0; i < n-1; i++){

            BasicBlock *ToMoveBB = UsedInstruction[i];
            Instruction *Clone = I->clone();
            
            Instruction *InsertBefore = findInsertionPoint(I, ToMoveBB);
            if (!InsertBefore){
              InsertBefore = ToMoveBB->getTerminator();
            }
            Clone->insertBefore(InsertBefore);
            Clone->setName(I->getName());
            
            // zamenimo sva koriscenja u ovom bloku sa kopijom
            for (User *U : I->users()){
              if (Instruction *UserI = dyn_cast<Instruction>(U)){
                if (UserI->getParent() == ToMoveBB) {
                  UserI->replaceUsesOfWith(I, Clone);
                }
              }
            }
          }

          BasicBlock *LastTarget = UsedInstruction.back();
          Instruction *InsertPoint = findInsertionPoint(I, LastTarget);

          if (!InsertPoint){
            InsertPoint = LastTarget->getTerminator();
          }

          I->moveBefore(InsertPoint);
          sinked = true;
        }
      }

      // nismo nista sinkovali
      return sinked;
    }


    bool runOnFunction(Function &F) override{

      Changed = false;
      for(BasicBlock &BB : F){
        Changed = Changed | sinking(&BB);
      }
      return Changed;
    }
  }; 
} 

char OurSinkPass::ID = 0;
static RegisterPass<OurSinkPass> X("our-sink", "Our simple implementation of SINK",
                             false,
                             false);
