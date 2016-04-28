#ifndef autosaver_h
#define autosaver_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Bert
 Date:		April 2016
________________________________________________________________________

-*/

#include "generalmod.h"
#include "monitor.h"
#include "multiid.h"
#include "uistring.h"
class IOObj;
class IOStream;


/*!\brief Object that with auto-save.

  You need to provide a fingerprint of your state when asked (prepare for this
  coming from another thread). If the fingerprint differs from previous time,
  you will be asked to store. The IOObj you get will be a modified version
  (always default local format). It will have a temporary IOObj ID.
  The actual fingerprint is up to you. Just make the chance of accidental
  match very low.

  You should tell the object when the user has (in some way) successfully
  saved the object, so next autosave can be postponed.

*/


namespace OD
{

class AutoSaveMgr;


/*!\brief Object that can be saved at any time. */

mExpClass(General) Saveable : public Monitorable
{
public:

			Saveable(const Monitorable&);
			~Saveable();
    const Monitorable&	monitored() const		{ return obj_; }

    mImplSimpleMonitoredGetSet(inline,key,setKey,MultiID,key_)
    inline		mImplSimpleMonitoredGet(isFinished,bool,objdeleted_)

    virtual bool	store(const IOObj&) const;
    uiString		errMsg() const		{ return errmsg_; }

protected:

    const Monitorable&	obj_;
    MultiID		key_;
    bool		objdeleted_;
    mutable uiString	errmsg_;

			// This function can be called from any thread
    virtual bool	doStore(const IOObj&) const	= 0;

private:

    void		objDel(CallBacker*);

};


/*!\brief Object that can be added to OD::AUTOSAVE(). */

mExpClass(General) AutoSaveable : public Saveable
{
public:

			AutoSaveable(const Monitorable&);
			~AutoSaveable();

    mImplSimpleMonitoredGetSet(inline,nrSecondsBetweenSaves,
				      setNrSecondsBetweenSaves,
				      int,nrclocksecondsbetweensaves_)

			// These functions can be called from any thread
    virtual BufferString getFingerPrint() const		= 0;

    void		userSaveOccurred() const;
    bool		saveNow() const			{ return doWork(true); }

    virtual bool	store(const IOObj&) const;
    virtual void	remove(const IOObj&) const;

protected:

    mutable BufferString prevfingerprint_;
    mutable IOStream*	prevstoreioobj_;
    mutable int		savenr_;
    mutable int		nrclocksecondsbetweensaves_;
    mutable int		lastsaveclockseconds_;
    mutable int		curclockseconds_;

    bool		doWork(bool forcesave) const;

private:

    void		removePrevStored() const;
    bool		needsAct(int) const;
    bool		act() const;
    friend class	AutoSaveMgr;

};


/*!\brief Auto-save manager. Singleton class.

  Work is done in its own thread.
  Saveable will automatically be removed when they are deleted.

  */

mExpClass(General) AutoSaveMgr : public CallBacker
{ mODTextTranslationClass(AutoSaveMgr)
public:

    bool		isActive(bool bydefault=false) const;
    void		setActive(bool,bool makedefault=true);
    int			nrSecondsBetweenSaves() const;
    void		setNrSecondsBetweenSaves(int);

    void		add(AutoSaveable*);

			// triggered from mgr's thread. CB obj is the saver.
    Notifier<AutoSaveMgr> saveDone;
    Notifier<AutoSaveMgr> saveFailed;

private:

			AutoSaveMgr();
			~AutoSaveMgr();

    ObjectSet<AutoSaveable> savers_;
    mutable Threads::Lock lock_;
    Threads::Thread*	thread_;
    bool		appexits_;
    bool		active_;

    void		appExits(CallBacker*);
    void		saverDel(CallBacker*);
    void		survChg( CallBacker* )	    { setEmpty(); }

    void		go();
    static inline void	goCB(CallBacker*);

public:

    // Probably not useful 4 u:

    static AutoSaveMgr&	getInst();		    // use OD::AUTOSAVE()
    void		remove(AutoSaveable*);	    // done 4 u on destruction
    void		setEmpty();		    // why would u?
    Threads::Thread&	thread()		    { return *thread_; }

};


mGlobal(General) inline AutoSaveMgr& AUTOSAVE()
{ return AutoSaveMgr::getInst(); }


}; //namespace OD

#endif
