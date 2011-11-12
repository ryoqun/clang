//===- IndexingContext.h - Higher level API functions ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Index_Internal.h"
#include "CXCursor.h"

#include "clang/AST/DeclObjC.h"
#include "clang/AST/DeclGroup.h"
#include "llvm/ADT/DenseMap.h"

namespace clang {
  class FileEntry;
  class ObjCPropertyDecl;
  class ObjCClassDecl;

namespace cxindex {
  class IndexingContext;

struct DeclInfo : public CXIdxDeclInfo {
  CXIdxEntityInfo CXEntInfo;
};

struct ObjCContainerDeclInfo : public DeclInfo {
  CXIdxObjCContainerDeclInfo ObjCContDeclInfo;
};

struct ObjCCategoryDeclInfo : public ObjCContainerDeclInfo {
  CXIdxObjCCategoryDeclInfo ObjCCatDeclInfo;
};

struct ObjCInterfaceDeclInfo : public ObjCCategoryDeclInfo {
  CXIdxObjCInterfaceDeclInfo ObjCInterDeclInfo;
};

struct ObjCProtocolDeclInfo : public ObjCCategoryDeclInfo {
  CXIdxObjCProtocolDeclInfo ObjCProtoDeclInfo;
};

class IndexingContext {
  ASTContext *Ctx;
  CXClientData ClientData;
  IndexerCallbacks &CB;
  unsigned IndexOptions;
  CXTranslationUnit CXTU;
  
  typedef llvm::DenseMap<const FileEntry *, CXIdxClientFile> FileMapTy;
  typedef llvm::DenseMap<const DeclContext *, CXIdxClientContainer> ContainerMapTy;
  FileMapTy FileMap;
  ContainerMapTy ContainerMap;

  SmallVector<DeclGroupRef, 8> TUDeclsInObjCContainer;
  
  llvm::SmallString<256> StrScratch;
  unsigned StrAdapterCount;

  class StrAdapter {
    llvm::SmallString<256> &Scratch;
    IndexingContext &IdxCtx;

  public:
    StrAdapter(IndexingContext &indexCtx)
      : Scratch(indexCtx.StrScratch), IdxCtx(indexCtx) {
      ++IdxCtx.StrAdapterCount;
    }

    ~StrAdapter() {
      --IdxCtx.StrAdapterCount;
      if (IdxCtx.StrAdapterCount == 0)
        Scratch.clear();
    }

    const char *toCStr(StringRef Str);

    unsigned getCurSize() const { return Scratch.size(); }

    const char *getCStr(unsigned CharIndex) {
      Scratch.push_back('\0');
      return Scratch.data() + CharIndex;
    }

    SmallVectorImpl<char> &getBuffer() { return Scratch; }
  };

  struct ObjCProtocolListInfo {
    SmallVector<CXIdxObjCProtocolRefInfo, 4> ProtInfos;
    SmallVector<CXIdxEntityInfo, 4> ProtEntities;
    SmallVector<CXIdxObjCProtocolRefInfo *, 4> Prots;

    CXIdxObjCProtocolRefInfo **getProtocolRefs() { return Prots.data(); }
    unsigned getNumProtocols() { return (unsigned)Prots.size(); }

    ObjCProtocolListInfo(const ObjCProtocolList &ProtList,
                         IndexingContext &IdxCtx,
                         IndexingContext::StrAdapter &SA);
  };

public:
  IndexingContext(CXClientData clientData, IndexerCallbacks &indexCallbacks,
                  unsigned indexOptions, CXTranslationUnit cxTU)
    : Ctx(0), ClientData(clientData), CB(indexCallbacks),
      IndexOptions(indexOptions), CXTU(cxTU), StrAdapterCount(0) { }

  ASTContext &getASTContext() const { return *Ctx; }

  void setASTContext(ASTContext &ctx);

  void enteredMainFile(const FileEntry *File);

  void ppIncludedFile(SourceLocation hashLoc,
                      StringRef filename, const FileEntry *File,
                      bool isImport, bool isAngled);

  void startedTranslationUnit();

  void indexDecl(const Decl *D);

  void indexTagDecl(const TagDecl *D);

  void indexTypeSourceInfo(TypeSourceInfo *TInfo, const NamedDecl *Parent,
                           const DeclContext *DC = 0);

  void indexTypeLoc(TypeLoc TL, const NamedDecl *Parent,
                           const DeclContext *DC);

  void indexDeclContext(const DeclContext *DC);
  
  void indexBody(const Stmt *S, const DeclContext *DC);

  void handleDiagnostic(const StoredDiagnostic &StoredDiag);

  void handleFunction(const FunctionDecl *FD);

  void handleVar(const VarDecl *D);

  void handleField(const FieldDecl *D);

  void handleEnumerator(const EnumConstantDecl *D);

  void handleTagDecl(const TagDecl *D);
  
  void handleTypedef(const TypedefDecl *D);

  void handleObjCClass(const ObjCClassDecl *D);
  void handleObjCInterface(const ObjCInterfaceDecl *D);
  void handleObjCImplementation(const ObjCImplementationDecl *D);

  void handleObjCForwardProtocol(const ObjCProtocolDecl *D,
                                 SourceLocation Loc,
                                 bool isRedeclaration);

  void handleObjCProtocol(const ObjCProtocolDecl *D);

  void handleObjCCategory(const ObjCCategoryDecl *D);
  void handleObjCCategoryImpl(const ObjCCategoryImplDecl *D);

  void handleObjCMethod(const ObjCMethodDecl *D);

  void handleObjCProperty(const ObjCPropertyDecl *D);

  void handleReference(const NamedDecl *D, SourceLocation Loc,
                       const NamedDecl *Parent,
                       const DeclContext *DC,
                       const Expr *E = 0,
                       CXIdxEntityRefKind Kind = CXIdxEntityRef_Direct);

  bool isNotFromSourceFile(SourceLocation Loc) const;

  void indexTUDeclsInObjCContainer();
  void indexDeclGroupRef(DeclGroupRef DG);

  void addTUDeclInObjCContainer(DeclGroupRef DG) {
    TUDeclsInObjCContainer.push_back(DG);
  }

  void translateLoc(SourceLocation Loc, CXIdxClientFile *indexFile, CXFile *file,
                    unsigned *line, unsigned *column, unsigned *offset);

private:
  void handleDecl(const NamedDecl *D,
                  SourceLocation Loc, CXCursor Cursor,
                  bool isRedeclaration, bool isDefinition, bool isContainer,
                  DeclInfo &DInfo);

  void handleObjCContainer(const ObjCContainerDecl *D,
                           SourceLocation Loc, CXCursor Cursor,
                           bool isForwardRef,
                           bool isRedeclaration,
                           bool isImplementation,
                           ObjCContainerDeclInfo &ContDInfo);

  void addContainerInMap(const DeclContext *DC, CXIdxClientContainer container);

  const NamedDecl *getEntityDecl(const NamedDecl *D) const;

  CXIdxClientContainer getIndexContainer(const NamedDecl *D) const {
    return getIndexContainerForDC(D->getDeclContext());
  }

  const DeclContext *getScopedContext(const DeclContext *DC) const;
  CXIdxClientContainer getIndexContainerForDC(const DeclContext *DC) const;

  CXIdxClientFile getIndexFile(const FileEntry *File);
  
  CXIdxLoc getIndexLoc(SourceLocation Loc) const;

  void getEntityInfo(const NamedDecl *D,
                     CXIdxEntityInfo &EntityInfo,
                     StrAdapter &SA);

  CXCursor getCursor(const NamedDecl *D) {
    return cxcursor::MakeCXCursor(const_cast<NamedDecl*>(D), CXTU);
  }

  CXCursor getRefCursor(const NamedDecl *D, SourceLocation Loc);
};

}} // end clang::cxindex