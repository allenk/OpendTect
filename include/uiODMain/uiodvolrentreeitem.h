#ifndef uiodvolrentreeitem_h
#define uiodvolrentreeitem_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Kristofer Tingdahl
 Date:          4-11-2002
 RCS:           $Id: uiodvolrentreeitem.h,v 1.12 2011-05-05 08:53:01 cvssatyaki Exp $
________________________________________________________________________


-*/

#include "uioddisplaytreeitem.h"

mClass uiODVolrenParentTreeItem : public uiTreeItem
{
    typedef uiTreeItem	inheritedClass;
public:
			uiODVolrenParentTreeItem();
			~uiODVolrenParentTreeItem();

			mMenuOnAnyButton;

    bool		showSubMenu();
    int			sceneID() const;

protected:
    bool		init();
    const char*		parentType() const;
};


mClass uiODVolrenTreeItemFactory : public uiODTreeItemFactory
{
public:
    const char*		name() const   { return getName(); }
    static const char*	getName();
    uiTreeItem*		create() const { return new uiODVolrenParentTreeItem; }
    uiTreeItem*		createForVis(int,uiTreeItem*) const;
};


mClass uiODVolrenTreeItem : public uiODDisplayTreeItem
{
public:
    			uiODVolrenTreeItem(int displayid_=-1);
    bool		showSubMenu();

protected:
			~uiODVolrenTreeItem();
    bool		init();
    BufferString	createDisplayName() const;
    uiODDataTreeItem*	createAttribItem( const Attrib::SelSpec* ) const;
    void		createMenuCB(CallBacker*);
    void		handleMenuCB(CallBacker*);
    bool		anyButtonClick( uiListViewItem* item );

    bool		isExpandable() const		{ return true; }
    const char*		parentType() const;

    bool		hasVolume() const;

    MenuItem		selattrmnuitem_;
    MenuItem            colsettingsmnuitem_;
    MenuItem		positionmnuitem_;
    MenuItem            statisticsmnuitem_;
    MenuItem            amplspectrummnuitem_;
    MenuItem		addmnuitem_;
    MenuItem		addlinlslicemnuitem_;
    MenuItem		addlcrlslicemnuitem_;
    MenuItem		addltimeslicemnuitem_;
    MenuItem		addvolumemnuitem_;
    MenuItem		addisosurfacemnuitem_;
    MenuItem		savevolumemnuitem_;
};


mClass uiODVolrenSubTreeItem : public uiODDisplayTreeItem
{
public:
    			uiODVolrenSubTreeItem(int displayid);

    bool		isVolume() const;
    bool		isIsoSurface() const;
    void		updateColumnText(int col);

protected:
			~uiODVolrenSubTreeItem();

    void		createMenuCB(CallBacker*);
    void		handleMenuCB(CallBacker*);

    bool		anyButtonClick( uiListViewItem* item );
    bool		init();
    const char*		parentType() const;

    MenuItem		resetisosurfacemnuitem_;
    MenuItem		convertisotobodymnuitem_;
};

#endif
