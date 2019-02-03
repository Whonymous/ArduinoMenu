/* -*- C++ -*- */
#pragma once
// a full automated SDCard file select
// plugin for arduino menu library
// requires a dynamic menu (MENU_USERAM)
// IO: Serial
// Feb 2019 - Rui Azevedo [ruihfazevedo@gmail.com]
#include <menu.h>

using namespace Menu;

// #ifdef MENU_USERAM

  template<typename SDC>
  class FSO {
  public:
    using Type=SDC;
    Type& sdc;
    //idx_t selIdx=0;//preserve selection context, because we preserve folder ctx too
    //we should use filename instead! idx is useful for delete operations thou...

    File dir;

    FSO(Type& sdc):sdc(sdc) {}
    virtual ~FSO() {dir.close();}
    //open a folder
    bool goFolder(String folderName) {
      dir.close();
      // Serial.println("reopen dir, context");
      dir=sdc.open(folderName.c_str());
      return dir;
    }
    //count entries on folder (files and dirs)
    idx_t count() {
      // Serial.print("count:");
      dir.rewindDirectory();
      int cnt=0;
      while(true) {
        File file=dir.openNextFile();
        if (!file) {
          file.close();
          break;
        }
        file.close();
        cnt++;
      }
      // Serial.println(cnt);
      return cnt;
    }

    //get entry index by filename
    idx_t entryIdx(String name) {
      dir.rewindDirectory();
      int cnt=0;
      while(true) {
        File file=dir.openNextFile();
        if (!file) {
          file.close();
          break;
        }
        if(name==file.name()) {
          file.close();
          return cnt;
        }
        file.close();
        cnt++;
      }
      return 0;//stay at menu start if not found
    }

    //get folder content entry by index
    String entry(idx_t idx) {
      dir.rewindDirectory();
      idx_t cnt=0;
      while(true) {
        File file=dir.openNextFile();
        if (!file) {
          file.close();
          break;
        }
        if(idx==cnt++) {
          String n=String(file.name())+(file.isDirectory()?"/":"");
          file.close();
          return n;
        }
        file.close();
      }
      return "";
    }

  };

  template<typename SDC>
  template<typename SDC>
  template<typename SDC>
////////////////////////////////////////////////////////////////////////////
  #include <SD.h>
  // instead of allocating options for each file we will instead customize a menu
  // to print the files list, we can opt to use objects for each file for a
  // faster reopening.. but its working quite fast
  // avr's will be limited to 127 file (not a good idea)
  // this can be changed without breaking compatibility thou, but wasting more memory.
  // On this example we assume the existence of an esc button as we are not drawing
  // an exit option (or [..] as would be appropriate for a file system)
  // not the mennu presents it self as the menu and as the options
  // ands does all drawing navigation.
  template<typename FS>
  class SDMenuT:public menuNode,public FS {
  public:
    String folderName="/";//set this to other folder when needed
    String selectedFile="";
    // using menuNode::menuNode;//do not use default constructors as we wont allocate for data
    SDMenuT(typename FS::Type& sd,constText* title,const char* at,Menu::action act=doNothing,Menu::eventMask mask=noEvent)
      :menuNode(title,0,NULL,act,mask,
        wrapStyle,(systemStyles)(_menuData|_canNav))
      ,FS(sd)
      // ,sdc(sd)
      // ,folderName(at)
      // ,dir(sdc.open(at))
      {
        // Serial.println("open dir, construction");
        // dir=sdc.open(at);
      }

    void begin() {FS::goFolder(folderName);}

    //this requires latest menu version to virtualize data tables
    prompt& operator[](idx_t i) const override {return *(prompt*)this;}//this will serve both as menu and as its own prompt
    result sysHandler(SYS_FUNC_PARAMS) override {
      switch(event) {
        case enterEvent:
          if (nav.root->navFocus!=nav.target) {//on sd card entry
            nav.sel=((SDMenuT<FS>*)(&item))->entryIdx(((SDMenuT<FS>*)(&item))->selectedFile);//restore context
          }
      }
      return proceed;
    }

    void doNav(navNode& nav,navCmd cmd) {
      switch(cmd.cmd) {
        case enterCmd: {
            String selFile=SDMenuT<FS>::entry(nav.sel);
            if (selFile.endsWith("/")) {
              // Serial.print("\nOpen folder...");
              //open folder (reusing the menu)
              folderName+=selFile;
              SDMenuT<FS>::goFolder(folderName);
              dirty=true;//redraw menu
              nav.sel=0;
            } else {
              //Serial.print("\nFile selected:");
              //select a file and return
              selectedFile=selFile;
              nav.root->node().event(enterEvent);
              menuNode::doNav(nav,escCmd);
            }
            return;
          }
        case escCmd:
          if(folderName=="/")//at root?
            menuNode::doNav(nav,escCmd);//then exit
          else {//previous folder
            idx_t at=folderName.lastIndexOf("/",folderName.length()-2)+1;
            String fn=folderName.substring(at,folderName.length()-1);
            folderName.remove(folderName.lastIndexOf("/",folderName.length()-2)+1);
            SDMenuT<FS>::goFolder(folderName);
            dirty=true;//redraw menu
            nav.sel=SDMenuT<FS>::entryIdx(fn);
          }
          return;
      }
      menuNode::doNav(nav,cmd);
    }

    //print menu and items as this is a virtual data menu
    Used printTo(navRoot &root,bool sel,menuOut& out, idx_t idx,idx_t len,idx_t pn) {
      if(root.navFocus!=this) {//show given title or filename if selected
        return selectedFile==""?
          menuNode::printTo(root,sel,out,idx,len,pn):
          out.printRaw(selectedFile.c_str(),len);
      } else if(idx==-1) {//when menu open (show folder name)
        ((menuNodeShadow*)shadow)->sz=SDMenuT<FS>::count();
        return out.printRaw(folderName.c_str(),len);
      }
      //drawing options
      len-=out.printRaw(SDMenuT<FS>::entry(idx).c_str(),len);
      return len;
    }
  };

  class SDMenu:public SDMenuT<FSO<decltype(SD)>> {
  public:
    SDMenu(constText* title,const char* at,Menu::action act=doNothing,Menu::eventMask mask=noEvent)
      :SDMenuT(SD,title,at,act,mask) {}
  };

// #endif
