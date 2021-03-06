/*===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 */
#include <klib/rc.h>
#include <klib/out.h>
#include <klib/text.h>
#include <klib/container.h>
#include <klib/refcount.h>
#include <klib/namelist.h>

#include <kfg/config.h>

#include <xfs/model.h>

#include "owp.h"
#include "mehr.h"
#include "zehr.h"
#include "schwarzschraube.h"

#include <sysalloc.h>

#include <stdio.h>
#include <string.h>

/*)))
 |||
 +++    Model and other
 |||
(((*/

/*)
 /      Useful farriables
(*/
static const char * _sXFSNodePath = "xfs";
static const char * _sXFSNodeModel = "model";
static const char * _sXFSModel_classname = "XFSModel";

/*)))
 |||
 +++    We suppose that there could exists several models, so ... Refs
 |||
(((*/
struct XFSModel {
    BSTree tree;
    KRefcount refcount;

    const char * Resource;
    const char * Version;
};

struct XFSModelNode {
    BSTNode node;

        /*)   the only property, which could not be overriden
         (*/
    const char * Name;

        /*)   all model node properties except 'children'
         (*/
    struct XFSOwp * Properties;

        /*)   there will be chidlren with labels
         (*/
    struct XFSOwp * Children;

    bool IsRoot;
};

/*)))
 (((    List of common properties for XFSModelNode, with some
  )))   explanations
 (((*/
#define XFS_MODEL_ROOT      "root"       /* the only mandatory node
                                            to have */
#define XFS_MODEL_AS        "as"         /* use this node as template
                                            overridden properties */
#define XFS_MODEL_TYPE      "type"       /* mandatory, used for tree
                                            rendering */
#define XFS_MODEL_LABEL     "label"      /* name which will be used at
                                            rendered tree, could be
                                            overriden by alias */
#define XFS_MODEL_SECURITY  "security"   /* in real life those are 
                                            permissions */
#define XFS_MODEL_CHILDREN  "children"   /* usually any container, it
                                            is list of names of children
                                            with labels */

/*)))
 ///    ModelNode Make and Destroy
(((*/

LIB_EXPORT
const char * CC
XFSModelNodeName ( const struct XFSModelNode * self )
{
    return self == NULL ? NULL : ( self -> Name );
}   /* XFSModelNodeName () */

LIB_EXPORT
const char * CC
XFSModelNodeType ( const struct XFSModelNode * self )
{
    return XFSModelNodeProperty ( self, XFS_MODEL_TYPE );
}   /* XFSModelNodeType () */

LIB_EXPORT
const char * CC
XFSModelNodeAs ( const struct XFSModelNode * self )
{
    return XFSModelNodeProperty ( self, XFS_MODEL_AS );
}   /* XFSModelNodeAs () */

LIB_EXPORT
const char * CC
XFSModelNodeLabel ( const struct XFSModelNode * self )
{
    return XFSModelNodeProperty ( self, XFS_MODEL_LABEL );
}   /* XFSModelNodeLabel () */

LIB_EXPORT
const char * CC
XFSModelNodeSecurity ( const struct XFSModelNode * self )
{
    return XFSModelNodeProperty ( self, XFS_MODEL_SECURITY );
}   /* XFSModelNodeSecurity () */

LIB_EXPORT
bool CC
XFSModelNodeIsRoot ( const struct XFSModelNode * self )
{
    const char * Name;

    Name = XFSModelNodeName ( self );

    if ( Name != NULL ) {
        if ( strcmp ( Name, XFS_MODEL_ROOT ) == 0 ) {
            return true;
        }
    }

    return Name == NULL
                    ? false
                    : ( strcmp ( Name, XFS_MODEL_ROOT ) == 0 )
                    ;
}   /* XFSModelNodeIsRoot () */

LIB_EXPORT
rc_t CC
XFSModelNodePropertyNames (
                    const struct XFSModelNode * self,
                    const struct KNamelist ** Properties
)
{
    if ( self == NULL || Properties == NULL ) {
        return XFS_RC ( rcNull );
    }

    * Properties = NULL;

    if ( self -> Properties == NULL ) {
        return 0;
    }

    return XFSOwpListKeys ( self -> Properties, Properties );
}   /* XFSModelNodeListProperties () */

LIB_EXPORT
const char * CC
XFSModelNodeProperty (
                    const struct XFSModelNode * self,
                    const char * PropertyName
)
{
    return self == NULL
                ? NULL
                : XFSOwpGet ( self -> Properties, PropertyName )
                ;
}   /* XFSModelNodeProperty () */

LIB_EXPORT
rc_t CC
XFSModelNodeChildrenNames (
                    const struct XFSModelNode * self,
                    const struct KNamelist ** Children
)
{
    if ( self == NULL || Children == NULL ) {
        return XFS_RC ( rcNull );
    }

    * Children = NULL;

    if ( self -> Children == NULL ) {
        return 0;
    }

    return XFSOwpListKeys ( self -> Children, Children );
}   /* XFSModelNodeChildrenNames () */

LIB_EXPORT
const char * CC
XFSModelNodeChildAlias (
                    const struct XFSModelNode * self,
                    const char * ChildName
)
{
    return self == NULL
                ? NULL
                : XFSOwpGet ( self -> Children, ChildName )
                ;
}   /* XFSModelNodeChildAlias () */

static
rc_t CC
_XFSModelNodeDispose ( struct XFSModelNode * Node )
{

printf ( "_XFSModelNodeDispose ( 0x%p, \"%s\" )\n", ( void * ) Node, ( Node -> Name == NULL ? "NULL" : ( Node -> Name ) ) );
    if ( Node == NULL ) {
            /* It is already disposed */
        return 0;
    }

    if ( Node -> Name != NULL ) {
        free ( ( char * ) Node -> Name );
        Node -> Name = NULL;
    }

    if ( Node -> Properties != NULL ) {
        XFSOwpDispose ( Node -> Properties );
        Node -> Properties = NULL;
    }

    if ( Node -> Children != NULL ) {
        XFSOwpDispose ( Node -> Children );
        Node -> Children = NULL;
    }

    free ( Node );

    return 0;
}   /* _XFSModelNodeDispose () */

static
rc_t CC
_XFSModelNodeMake ( const char * Name, struct XFSModelNode ** Node )
{
    rc_t RCt;
    struct XFSModelNode * Knoten;

    RCt = 0;

    if ( Name == NULL || Node == NULL ) {
        return XFS_RC ( rcNull );
    }

    * Node = NULL;

    Knoten = calloc ( 1, sizeof ( struct XFSModelNode ) );
    if ( Knoten == NULL ) {
        return XFS_RC ( rcExhausted );
    }

    Knoten -> IsRoot = strcmp ( Name, XFS_MODEL_ROOT ) == 0;

    RCt = XFS_StrDup ( Name, & ( Knoten -> Name ) );
    if ( RCt == 0 ) {
        RCt = XFSOwpMake ( & ( Knoten -> Properties ) );

        if ( RCt == 0 ) {
            RCt = XFSOwpMake ( & ( Knoten -> Children ) );

            if ( RCt == 0 ) {
                * Node = Knoten;
            }
        }
    }

    if ( RCt != 0 ) {
        _XFSModelNodeDispose ( Knoten );

        Knoten = NULL;
    }

printf ( "_XFSModelNodeMake ( 0x%p, \"%s\" )\n", ( void * ) * Node, Name );

    return RCt;
}   /* _XFSModeNodeMake () */



/*)))
 ///    Model Make and Destroy
(((*/

/*))
 //   That method will return source for model ... need to rethink
((*/
static
rc_t CC
_GetDefaultModelSource ( const char ** Source )
{
    rc_t RCt;
    const struct KConfig * Konfig;
    const struct KConfigNode * KonfigNode;
    char Buf [ XFS_SIZE_4096 ];
    size_t Readed;

    RCt = 0;
    Konfig = NULL;
    KonfigNode = NULL;
    Readed = 0;

    if ( Source == NULL ) {
        return XFS_RC ( rcNull );
    }

    * Source = NULL;

    Konfig = XFS_Config_MHR ();
    if ( Konfig != NULL ) {
        RCt = KConfigOpenNodeRead (
                                Konfig,
                                & KonfigNode,
                                "%s/%s",
                                _sXFSNodePath,
                                _sXFSNodeModel
                                );
        if ( RCt == 0 ) {
            RCt = KConfigNodeRead (
                                    KonfigNode,
                                    0,
                                    Buf,
                                    sizeof ( Buf ),
                                    & Readed,
                                    NULL
                                    );
            if ( RCt == 0 && 0 < Readed ) {
                * Source = ( const char * ) string_dup ( Buf, Readed );
            }

            KConfigNodeRelease ( KonfigNode );
        }
    }
    else {
        RCt = XFS_RC ( rcInvalid );
    }

    return RCt;
}   /* _GetDefaultModelSource () */

/*))
 //   That method will create and initialize model from scratch
((*/
static
rc_t CC
_ErstellenUndInitialisierenModel (
                    const char * Resource,
                    const char * Version,
                    const struct XFSModel  ** Model
)
{
    struct XFSModel * Modell;

    Modell = NULL;

    if ( Resource == NULL || Model == NULL ) {
        return XFS_RC ( rcNull );
    }

    * Model = NULL;

    Modell = calloc ( 1, sizeof ( struct XFSModel ) );
    if ( Modell == NULL ) {
        return XFS_RC ( rcExhausted );
    }

    BSTreeInit ( & ( Modell -> tree ) );
    KRefcountInit (
                    & ( Modell -> refcount ),
                    1,
                    _sXFSModel_classname,
                    "XFSModelMake",
                    "Model"
                    );
    if ( Version != NULL ) {
        XFS_StrDup ( Version, & ( Modell -> Version ) );
    }

    if ( XFS_StrDup ( Resource, & ( Modell -> Resource ) ) != 0 ) {
            /* We should do it ... */
        XFSModelDispose ( Modell );

        * Model = NULL;     /* just for case */

        return XFS_RC ( rcInvalid );
    }

    * Model = Modell;

    return 0;
}   /* _ErstellenUndInitialisierenModel () */

/*))
 //   Some usual
((*/

    /*))
     ((   There is very simple format for children field value:
      ))      name[:label][,name[:label]....]
     ((*/
static
rc_t CC
_ParseAddNodeChildren (
                const struct XFSModelNode * Node,
                const char * ChildrenProperty
)
{
    rc_t RCt;
    struct KNamelist * Children;
    struct KNamelist * Break;
    uint32_t ChildCount, llp;
    const char * Child;
    uint32_t BreakCount, ppl;
    const char * BChild, * BAlias;

    RCt = 0;
    ChildCount = llp = 0;
    Children = NULL;
    Child = NULL;
    BreakCount = ppl = 0;
    Break = NULL;
    BChild = BAlias = NULL;

    RCt = XFS_SimpleTokenize_ZHR ( ChildrenProperty, ',', & Children );
    if ( RCt == 0 ) {
        RCt = KNamelistCount ( Children, & ChildCount );
        if ( RCt == 0 ) {
            for ( llp = 0; llp < ChildCount; llp ++ ) { 
                RCt = KNamelistGet ( Children, llp, & Child );
                if ( RCt == 0 ) {
                    RCt = XFS_SimpleTokenize_ZHR ( Child, ':', &Break );
                    if ( RCt == 0 ) {
                        RCt = KNamelistCount ( Break, & BreakCount );
                        if ( RCt == 0 ) {
                            BChild = BAlias = NULL;

                            RCt = KNamelistGet ( Break, 0, & BChild );
                            if ( RCt == 0 ) {
                                if ( 1 < BreakCount ) {
                                    RCt = KNamelistGet (
                                                    Break,
                                                    1,
                                                    & BAlias
                                                    );
                                }
                            }
                            if ( RCt == 0 ) {
                                RCt = XFSOwpSet (
                                                    Node -> Children,
                                                    BChild,
                                                    BAlias
                                                    );
                            }
                        }

                        KNamelistRelease ( Break );
                    }
                }

                if ( RCt != 0 ) {
                    break;
                }
            }
        }

        KNamelistRelease ( Children );
    }

    return RCt;
}   /* _ParseAddNodeChildren () */

static
rc_t CC
_SetModelNodeProperty (
            const struct XFSModelNode * ModelNode,
            const struct KConfigNode * KonfigNode,
            const char * PropertyName
)
{
    rc_t RCt;
    struct KConfigNode * Node;
    char Buv [ XFS_SIZE_1024 ];
    size_t NumRead, Remain;

    RCt = 0;
    Node = NULL;
    NumRead = Remain = 0;

        /* First we opening node for read */
    RCt = KConfigNodeOpenNodeRead (
                            KonfigNode,
                            ( const struct KConfigNode ** ) & Node,
                            PropertyName
                            );
    if ( RCt == 0 ) {
        RCt = KConfigNodeRead (
                            Node,
                            0,
                            Buv,
                            sizeof ( Buv ),
                            & NumRead,
                            & Remain
                            );
        if ( RCt == 0 ) {
            Buv [ NumRead ] = 0;

            if ( strcmp ( PropertyName, XFS_MODEL_CHILDREN ) == 0 ) {
                _ParseAddNodeChildren ( ModelNode, Buv );
            }
            else {
                RCt = XFSOwpSet (
                                    ModelNode -> Properties,
                                    PropertyName,
                                    Buv
                                    );
            }
        }

        KConfigNodeRelease ( Node );
    }

    return RCt;
}   /* _SetModelNodeProperty () */

static
int CC
_LoadNodeCallback ( const BSTNode * N1, const BSTNode * N2 )
{
    return XFS_StringCompare4BST_ZHR (
                            ( ( struct XFSModelNode * ) N1 ) -> Name,
                            ( ( struct XFSModelNode * ) N2 ) -> Name
                            );
}   /* _LoadNodeCallback () */

/*))
 //   That method will load node for moded by name
((*/
static 
rc_t CC
_LoadModelNode (
            const struct KConfig * Konfig,
            struct XFSModel * Model,
            const char * Name
)
{
    rc_t RCt;
    struct KConfigNode * Node;
    struct XFSModelNode * Mode;
    struct KNamelist * List;
    uint32_t ListSize, llp;
    const char * ListEntry;

    RCt = 0;
    Node = NULL;
    ListEntry = NULL;
    ListSize = llp = 0;

        /* Here we do not do checks for NULL .... it is too deep */

        /*) First, all nodes are unique, so we are leaving if here
         /  already exists definition for node with that name
        (*/
    if ( XFSModelLookupNode ( Model, Name ) ) {
        return 0;
    }

    RCt = KConfigOpenNodeRead (
                            Konfig,
                            ( const struct KConfigNode ** ) & Node,
                            Name
                            );
    if ( RCt == 0 ) {
        RCt = _XFSModelNodeMake ( Name, & Mode );
        if ( RCt == 0 ) {
            RCt = KConfigNodeListChildren ( Node, & List );
            if ( RCt == 0 ) {
                RCt = KNamelistCount ( List, & ListSize );
                if ( RCt == 0 ) {
                    for ( llp = 0; llp < ListSize; llp ++ ) {
                        RCt = KNamelistGet ( List, llp, & ListEntry );
                        if ( RCt == 0 ) {
                            RCt = _SetModelNodeProperty (
                                                        Mode,
                                                        Node,
                                                        ListEntry
                                                        );
                        }
                        if ( RCt != 0 ) {
                            break;
                        }
                    }
                }

                KNamelistRelease ( List );
            }
        }


        KConfigNodeRelease ( Node );
    }

    if ( RCt == 0 ) {
        RCt = BSTreeInsert (
                        & ( Model -> tree ),
                        ( BSTNode * ) Mode,
                        _LoadNodeCallback
                        );
        if ( RCt == 0 ) {
            RCt = XFSModelNodeChildrenNames (
                                    Mode,
                                    ( const struct KNamelist ** ) & List
                                    );
            if ( RCt == 0 && List != NULL ) {
                RCt = KNamelistCount ( List, & ListSize );
                if ( RCt == 0 ) {
                    for ( llp = 0; llp < ListSize; llp ++ ) {
                        RCt = KNamelistGet ( List, llp, & ListEntry );
                        if ( RCt == 0 ) {
                            RCt = _LoadModelNode (
                                                Konfig,
                                                Model,
                                                ListEntry
                                                );

                            if ( RCt != 0 ) {
                                break;
                            }
                        }
                    }
                }

                KNamelistRelease ( List );
            }
        }
    }

    return RCt;
}   /* _LoadModelNode () */

/*))
 //   That method will load model from resource by name
((*/
static
rc_t CC
_LoadModel ( struct XFSModel  * Model )
{
    rc_t RCt;
    const struct KConfig * Config;

    RCt = 0;
    Config = NULL;

    if ( Model == NULL ) {
        return XFS_RC ( rcNull );
    }

    if ( Model -> Resource == NULL ) {
        return XFS_RC ( rcInvalid );
    }

    RCt = XFS_LoadConfig_ZHR ( Model -> Resource, & Config );
    if ( RCt == 0 ) {
            /* First we should check if here is node with name 'root'
             */
        RCt = _LoadModelNode ( Config, Model, XFS_MODEL_ROOT );

        KConfigRelease ( Config );
    }

    return RCt;
}   /* _LoadModel () */

/*
 *   Creates model, and load it from Source. 
 *   Source could be NULL, in that case it will load it from config
 */
LIB_EXPORT
rc_t CC
XFSModelMake (
            struct XFSModel ** Model,
            const char * Resource,
            const char * Version
)
{
    rc_t RCt;
    const char * ModelResource;
    bool DefaultModelResource;

    RCt = 0;
    DefaultModelResource = false;

    if ( Model == NULL ) {
        return XFS_RC ( rcNull );
    }

    if ( Resource == NULL ) {
        RCt = _GetDefaultModelSource ( & ModelResource );
        DefaultModelResource = true;
    }
    else {
        ModelResource = Resource;
    }

    if ( RCt == 0 ) {
        RCt = _ErstellenUndInitialisierenModel (
                                    ModelResource,
                                    Version,
                                    ( const struct XFSModel ** ) Model
                                    );
        if ( RCt == 0 ) {
            RCt = _LoadModel ( * Model );
            if ( RCt != 0 ) {
                XFSModelDispose ( ( struct XFSModel * ) * Model );
                * Model = NULL;
            }
        }
    }

    if ( DefaultModelResource ) {
        free ( ( char * ) ModelResource );
        ModelResource = NULL;
    }

printf ( "_XFSModelMake ( 0x%p )\n", ( void * ) * Model );

    return RCt;
}   /* XFSModelMake () */

static
void CC
_ModelWhackCallback ( BSTNode * Node, void * Unused )
{
    if ( Node != NULL ) {
        _XFSModelNodeDispose ( ( struct XFSModelNode * ) Node );
    }
}   /* _ModelWhackCallback () */

LIB_EXPORT
rc_t CC
XFSModelDispose ( struct XFSModel * self )
{
printf ( "_XFSModelDispose ( 0x%p )\n", ( void * ) self );

    if ( self == NULL ) {
            /* Nothing to dispose */
        return 0;
    }

    if ( self -> Version != NULL ) {
        free ( ( char * ) self -> Version );
        self -> Version = NULL;
    }

    if ( self -> Resource != NULL ) {
        free ( ( char * ) self -> Resource );
        self -> Resource = NULL;
    }

        /*) by BSTree code: it is safe not to check tree->root == NULL
         (*/
    BSTreeWhack ( ( BSTree * ) self, _ModelWhackCallback, NULL );

    KRefcountWhack ( & ( self -> refcount ), _sXFSModel_classname );

    free ( self );

    return 0;
}   /* XFSModelDispose () */

LIB_EXPORT
rc_t CC
XFSModelAddRef ( const struct XFSModel * self )
{
    rc_t RCt;

    RCt = 0;

    if ( self != NULL ) {
        switch ( KRefcountAdd (
                            & ( self -> refcount ),
                            _sXFSModel_classname
                            )
        ) {
            case krefOkay :
                        RCt = 0;
                        break;
            case krefZero :
            case krefLimit :
            case krefNegative :
                        RCt = XFS_RC ( rcInvalid );
                        break;
            default :
                        RCt = XFS_RC ( rcUnknown );
                        break;
        }
    }
    else {
        RCt = XFS_RC ( rcNull );
    }

    return RCt;
}   /* XFSModelAddRef () */

LIB_EXPORT
rc_t CC
XFSModelRelease ( const struct XFSModel * self )
{
    rc_t RCt;

    RCt = 0;

    if ( self != NULL ) {
        switch ( KRefcountDrop (
                            & ( self -> refcount ),
                            _sXFSModel_classname
                            )
        ) {
            case krefOkay :
            case krefZero :
                        RCt = 0;
                        break;
            case krefWhack :
                        RCt = XFSModelDispose ( ( struct XFSModel * ) self );
                        break;
            case krefNegative :
                        RCt = XFS_RC ( rcInvalid );
                        break;
            default :
                        RCt = XFS_RC ( rcUnknown );
                        break;
        }
    }

    return RCt;
}   /* XFSModelRelease () */

LIB_EXPORT
const struct XFSModelNode * CC
XFSModelRootNode ( const struct XFSModel * self )
{
    return XFSModelLookupNode ( self, XFS_MODEL_ROOT );
}   /* XFSModelRootNode () */

static
int CC
_LookupNodeCallback ( const void * Item, const BSTNode * Node )
{
    return XFS_StringCompare4BST_ZHR (
                        ( const char * ) Item,
                        ( ( struct XFSModelNode * ) Node ) -> Name
                        );
}   /* _LookupNodeCallback () */

LIB_EXPORT
const struct XFSModelNode * CC
XFSModelLookupNode ( const struct XFSModel * self, const char * Name )
{
    if ( self == NULL || Name == NULL ) {
        return NULL;
    }

    return ( const struct XFSModelNode * ) BSTreeFind (
                                                & ( self -> tree ),
                                                Name,
                                                _LookupNodeCallback
                                                );
}   /* XFSModelLookupNode () */

LIB_EXPORT
const char * CC
XFSModelResource ( const struct XFSModel * self )
{
    return self == NULL ? NULL : ( self -> Resource );
}   /* XFSModelResource () */

LIB_EXPORT
const char * CC
XFSModelVersion ( const struct XFSModel * self )
{
    return self == NULL ? NULL : ( self -> Version );
}   /* XFSModelVersion () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
LIB_EXPORT
void CC
XFSModelNodeDDump ( const struct XFSModelNode * self )
{
    const struct KNamelist * List;
    uint32_t Count, llp;
    const char * Key, * Prop;

    List = NULL;
    Count = llp = 0;
    Key = Prop = NULL;

    if ( self == NULL ) {
        printf ( "   NODE [null]\n" );
        return;
    }

    printf ( "  NODE [%s]\n", self -> Name );

    if ( XFSModelNodePropertyNames ( self, & List ) == 0 ) {
        if ( List != NULL ) {
            if ( KNamelistCount ( List, & Count ) == 0 ) {
                if ( Count == 0 ) {
                    printf ( "    PROPERTIES [NONE]\n" );
                }
                else {
                    printf ( "    PROPERTIES [#%d]\n", Count );
                    for ( llp = 0; llp < Count; llp ++ ) {
                        if ( KNamelistGet ( List, llp, & Key ) == 0 ) {
                            Prop = XFSModelNodeProperty ( self, Key );
                            printf ( "      [%s][%s]\n"
                                            , Key
                                            , Prop == NULL ? "null" : Prop
                                            ); 
                        }
                    }
                }
            }
            else {
                printf ( "    PROPERTIES [NONE]\n" );
            }

            KNamelistRelease ( List );
            List = NULL;
        }
    }

        /* Children */
    if ( XFSModelNodeChildrenNames ( self, & List ) == 0 ) {
        if ( List != NULL ) {
            if ( KNamelistCount ( List, & Count ) == 0 ) {
                if ( Count == 0 ) {
                    printf ( "    CHILDREN [NONE]\n" );
                }
                else {
                    printf ( "    CHILDREN [#%d]\n", Count );
                    for ( llp = 0; llp < Count; llp ++ ) {
                        if ( KNamelistGet ( List, llp, & Key ) == 0 ) {
                            Prop = XFSModelNodeChildAlias ( self, Key );
                            if ( Prop == NULL ) {
                                printf ( "      [%s]\n" , Key ); 
                            }
                            else {
                                printf ( "      [%s][%s]\n" , Key , Prop ); 
                            }
                        }
                    }
                }
            }
            else {
                printf ( "    CHILDREN [NONE]\n" );
            }

            KNamelistRelease ( List );
            List = NULL;
        }
    }

}   /* XFSModelNodeDDump () */

static
void CC
_ModelDDumpCallback ( BSTNode * Node, void * Data )
{
    XFSModelNodeDDump ( ( const struct XFSModelNode * ) Node );
}   /* _ModelDDumpCallback () */

LIB_EXPORT
void CC
XFSModelDDump ( const struct XFSModel * self )
{
    if ( self == NULL ) {
        printf ( "MODEL [null]\n" );
        return;
    }
    printf ( "MODEL Resource[%s] Version[%s]\n",
                     self -> Resource,
                     self -> Version == NULL ? "null": self -> Version
                     );

    BSTreeForEach (
                & ( self -> tree ),
                false,
                _ModelDDumpCallback,
                NULL
                );
}   /* XFSModelDDump () */
