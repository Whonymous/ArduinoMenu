
#include "menu.h"
using namespace Menu;

result Menu::doNothing() {return proceed;}
result Menu::doExit() {return quit;}
action Menu::noAction(doNothing);

//this is for idle (menu suspended)
result Menu::inaction(menuOut& o,idleEvent) {return proceed;}

idx_t prompt::printRaw(menuOut& out,idx_t len) const {
  return print_P(out,getText(),len);
}

idx_t prompt::printTo(navRoot &root,bool sel,menuOut& out, idx_t idx,idx_t len,idx_t panelNr) {
  out.fmtStart(menuOut::fmtPrompt,root.node(),idx);
  idx_t r=printRaw(out,len);
  out.fmtEnd(menuOut::fmtPrompt,root.node(),idx);
  return r;
}

prompt* menuNode::seek(idx_t* uri,idx_t len) {
  if (len&&uri[0]>=0&&uri[0]<sz()) {
    prompt& e=operator[](uri[0]);
    assert(e.isMenu());
    return e.seek(++uri,--len);
  } else return NULL;
}
bool menuNode::async(const char *uri,navRoot& root,idx_t lvl) {
  //Serial<<*(prompt*)this<<" menuNode::async "<<uri<<" lev:"<<lvl<<endl;
  if ((!*uri)||(uri[0]=='/'&&!uri[1])) return this;
  uri++;
  idx_t n=0;
  while (*uri) {
    char* d=strchr(numericChars,uri[0]);
    if (d) n=n*10+((*d)-'0');
    else break;
    uri++;
  }
  if (root.path[lvl].target!=this) {
    //Serial<<"escaping"<<endl;
    while(root.level>lvl) root.doNav(escCmd);
  }
  //Serial<<*(prompt*)this<<" doNav idxCmd:"<<n<<endl;Serial.flush();
  //if (this->operator[](n).type()!=fieldClass) {//do not enter edit mode on fields over async
    //Serial<<"doNav idxCmd"<<endl;
    root.doNav(navCmd(idxCmd,n));
  //}
  //Serial<<"recurse on ["<<n<<"]-"<<operator[](n)<<" uri:"<<uri<<" lvl:"<<lvl<<endl;Serial.flush();
  return operator[](n).async(uri,root,++lvl);
}

void textField::doNav(navNode& nav,navCmd cmd) {
  //Serial.println("textField doNav!");
  switch(cmd.cmd) {
    case enterCmd:
      if (edited&&!charEdit) {
        edited=false;
        cursor=0;
        nav.root->exit();
      } else {
        charEdit=!charEdit;
        dirty=true;
        edited=true;
      }
      break;
    case escCmd:
      dirty=true;
      if (charEdit) charEdit=false;
      else {
        edited=false;
        cursor=0;
        nav.root->exit();
      }
      break;
    case upCmd:
      if (charEdit) {
        const char* v=validator(cursor);
        char *at=strchr(v,buffer()[cursor]);
        idx_t pos=at?at-v+1:1;
        if (pos>=strlen(v)) pos=0;
        buffer()[cursor]=v[pos];
        dirty=true;
      } else {
        if(cursor<strlen(buffer())-1) cursor++;
        edited=false;
      }
      dirty=true;
      break;
    case downCmd:
      if (charEdit) {
        const char* v=validator(cursor);
        char *at=strchr(v,buffer()[cursor]);//at is not stored on the class to save memory
        idx_t pos=at?at-v-1:0;
        if (pos<0) pos=strlen(v)-1;
        buffer()[cursor]=v[pos];
        dirty=true;
      } else {
        if (cursor) cursor--;
        edited=false;
      }
      dirty=true;
      break;
    default:break;
  }
}

idx_t textField::printTo(navRoot &root,bool sel,menuOut& out, idx_t idx,idx_t len,idx_t panelNr) {
  // out.fmtStart(menuOut::fmtPrompt,root.node(),idx);
  idx_t at=0;
  bool editing=this==root.navFocus;
  idx_t l=navTarget::printTo(root,sel,out,idx,len,panelNr);
  if (l<len) {
    out.write(editing?":":" ");
    l++;
  }
  idx_t c=l;
  //idx_t top=out.tops[root.level];
  idx_t tit=root.showTitle?1:0;
  idx_t line=idx+tit;
  while(buffer()[at]&&l++<len)
    if (at==cursor&&editing) {
      c=l;
      l+=out.startCursor(root,c,line,charEdit);//draw textual cursor or color code start
      out.write(buffer()[at++]);//draw focused character
      l+=out.endCursor(root,c,line,charEdit);//draw textual cursor or color code end
    } else out.write(buffer()[at++]);
  l+=out.editCursor(root,c,line,editing,charEdit);//reposition a non text cursor
  return l;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// menuOut - base menu output device
//
////////////////////////////////////////////////////////////////////////////////
idx_t menuOut::printRaw(const char* at,idx_t len) {
  const char* p=at;
  uint8_t ch;
  for(int n=0;(ch=*(at++))&&(len==0||n<len);n++) {
    write(ch);
  }
  return at-p;
}

void menuOut::doNav(navCmd cmd,navNode &nav) {
  panel p=panels[nav.root->level];
  idx_t t=top(nav)-1;
  idx_t sz=nav.target->sz();
  switch(cmd.cmd) {
    case scrlUpCmd:
      if (t) {
        t--;
        if (nav.sel>t+sz) nav.sel--;
      }
      break;
    case scrlDownCmd:
      if (t+p.h<sz) {
        t++;
        if (nav.sel<t) nav.sel++;
      }
      break;
    default: assert(false);
  }
}

#ifdef DEBUG
menuOut& menuOut::operator<<(const prompt& p) {
  print_P(*this,p.getText());
  return *this;
}
#endif

void outputsList::printMenu(navNode& nav) const {
  for(int n=0;n<cnt;n++) {
    menuOut& o=*((menuOut*)memPtr(outs[n]));
    //Serial<<endl;
    //print_P(Serial,nav.target->getText());
    //Serial<<" "<<n<<" changed: "<<nav.changed(o);
    if (nav.changed(o)||(o.style&(menuOut::rasterDraw)))
      o.printMenu(nav);
  }
  clearChanged(nav);
  /*for(int n=0;n<cnt;n++) {
    menuOut& o=*((menuOut*)memPtr(outs[n]));
    Serial<<endl<<n<<" after: "<<nav.changed(o)<<endl;
  }*/
}

void menuOut::clearChanged(navNode &nav) {
  //if (nav.target->dirty) Serial<<"clear dirty "<<*(prompt*)nav.target<<" sz:"<<nav.sz()<<endl;
  nav.target->dirty=false;
  //Serial<<"clearChanged ";
  idx_t t=tops[nav.root->level];
  for(idx_t i=0;i<maxY();i++,t++) {//only signal visible
    //Serial<<"["<<t<<"]";
    if (t>=nav.sz()) break;//menu ended
    nav[t].dirty=false;
    //Serial<<".";
  }
  //Serial<<" "<<nav.target->dirty<<endl;
}

// draw a menu preview on a panel
void menuOut::previewMenu(navRoot& root,menuNode& menu,idx_t panelNr) {
  clear(panelNr);
  setColor(fgColor,false);
  setCursor(0,0,panelNr);
  for(idx_t i=0;i<maxY(panelNr);i++) {
    if (i>=menu.sz()) break;
    prompt& p=menu[i];
    clearLine(i,panelNr,bgColor,false,p.enabled);
    setCursor(0,i,panelNr);
    setColor(fgColor,false,p.enabled);
    drawCursor(i,false,p.enabled,false,panelNr);
    setColor(fgColor,false,p.enabled,false);
    p.printTo(root,false,*this,i,panels[panelNr].w,panelNr);
  }
}

//determin panel number here and distribute menu and previews among the panels
void menuOut::printMenu(navNode &nav) {
  menuNode& focus=nav.root->active();
  int lvl=nav.root->level;
  if (focus.parentDraw()) lvl--;
  navNode& nn=nav.root->path[lvl];
  int k=min(lvl,panels.sz-1);
  if ((style&usePreview)&&k) k--;
  for(int i=0;i<k;i++) {
    navNode &n=nav.root->path[lvl-k+i];
    if (!((style&minimalRedraw)&&panels.nodes[i]==&n)) {
      previewMenu(*nav.root,*n.target,i);
      panels.nodes[i]=&n;
    }
  }
  panels.cur=k;
  printMenu(nn,k);
  panels.nodes[k]=&nav;//for cleaning purposes
  for(int i=k+2;i<panels.sz;i++) if (panels.nodes[i]) {
    clear(i);
    panels.nodes[i]=NULL;
  }
}

// generic (menuOut) print menu on a panel
// this function emits format messages
// to be handler by format wrappers
void menuOut::printMenu(navNode &nav,idx_t panelNr) {
  //menuNode& focus=nav.root->active();
  if (!(nav.root->navFocus->parentDraw()||nav.root->navFocus->isMenu())) {
    //on this case we have a navTarget object that draws himself
    if (nav.root->navFocus->changed(nav,*this,false))
      nav.root->navFocus->printTo(*nav.root,true,*this,nav.sel,maxX(panelNr),panelNr);
  } else {
    idx_t topi=nav.root->level-((nav.root->active().parentDraw())?1:0);
    idx_t ot=tops[topi];
    bool asPad=((prompt*)nav.target)->sysStyles()&_asPad;
    idx_t st=(style&showTitle)&&(nav.root->showTitle&&(asPad||(maxY(panelNr)>1)))?1:0;//do not use titles on single line devices!
    while(nav.sel+st>=(tops[topi]+maxY(panelNr))) tops[topi]++;
    while(nav.sel<tops[topi]||(tops[topi]&&((nav.sz()-tops[topi])<maxY(panelNr)-st))) tops[topi]--;
    bool all=(style&redraw)
      ||(tops[topi]!=ot)
      ||(drawn!=nav.target)
      ||(panels.nodes[panelNr]!=&nav);
    if (!(all||(style&minimalRedraw))) //non minimal draw will redraw all if any change
      all|=nav.target->changed(nav,*this);
    all|=nav.target->dirty;
    if (!(all||(style&minimalRedraw))) return;
    panel pan=panels[panelNr];

    //-----> panel start
    fmtStart(fmtPanel,nav);
    if (all) {
      clear(panelNr);
      if (st) {
        ///------> titleStart
        fmtStart(fmtTitle,nav,-1);
        setColor(titleColor,false);
        clearLine(0,panelNr);
        setColor(titleColor,true);
        setCursor(0,0,panelNr);
        //print('[');
        nav.target->prompt::printTo(*nav.root,true,*this,-1,pan.w-1,panelNr);
        if (asPad) print(":");
        //print(']');
        ///<----- titleEnd
        fmtEnd(fmtTitle,nav,-1);
      }
    }
    //------> bodyStart
    fmtStart(fmtBody,nav);
    for(idx_t i=0;i<maxY(panelNr)-st;i++) {
      int ist=i+st;
      if (i+tops[topi]>=nav.sz()) break;
      prompt& p=nav[i+tops[topi]];
      idx_t len=pan.w;
      if (all||p.changed(nav,*this,false)) {
        //-------> opStart
        fmtStart(fmtOp,nav,i);
        bool selected=nav.sel==i+tops[topi];
        bool ed=nav.target==&p;
        //-----> idxStart
        fmtStart(fmtIdx,nav,i);
        if (!asPad) {//pad MUST be able of clearing part of a line!!!!!
          clearLine(ist,panelNr,bgColor,selected,p.enabled);//<-- THIS IS DEPENDENT OF A DROP MENU!!!!
          setCursor(0,ist,panelNr);//<-- THIS IS MAKING A DROP MENU!!!!
        }
        setColor(fgColor,selected,p.enabled,ed);
        //should we use accels on pads?
        if ((!asPad)&&(drawNumIndex&style)) {//<-- NO INDEX FOR PADS
          char a=tops[topi]+i+'1';
          print('[');
          print(a<='9'?a:'-');
          print(']');
          len-=3;
        }
        //<------idxEnd
        fmtEnd(fmtIdx,nav,i);
        //------> cursorStart
        fmtStart(fmtCursor,nav,i);
        if (asPad&&selected) print("[");
        else drawCursor(ist,selected,p.enabled,ed,panelNr);//assuming only one character
        //<------ cursorEnd
        fmtEnd(fmtCursor,nav,i);
        len--;
        //---->opBodyStart
        fmtStart(fmtOpBody,nav,i);
        setColor(fgColor,selected,p.enabled,ed);
        if (len>0) len=p.printTo(*nav.root,selected,*this,i,len,panelNr);
        if (len>0) {
          if (asPad) {
            print(selected?"]":" ");
            len--;
          }
        }
        //<---opBodyEnd
        fmtEnd(fmtOpBody,nav,i);
        if (selected&&panels.sz>panelNr+1) {
          if(p.isMenu()) {
            //----->  previewStart
            previewMenu(*nav.root,*(menuNode*)&p,panelNr+1);
            panels.nodes[panelNr+1]=&nav;
            //<----  previewEnd
          } else if (panels.nodes[panelNr+1]) clear(panelNr+1);
        }
        //opEnd
        fmtEnd(fmtOp,nav,i);
      }
    }
  }
  //<-----bodyEnd
  fmtEnd(fmtBody,nav,-1);
  //if (any) Serial<<"printMenu "<<*(prompt*)nav.target<<endl;
  drawn=nav.target;
  //lastSel=nav.sel;
  //<---- panel end
  fmtEnd(fmtPanel,nav,-1);
}

//here we use navRoot pointer for all navNodes
// => THERE CAN ONLY BE ONE navRoot
navRoot* navNode::root=NULL;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// menuNode - any menu object that has a list of members
//
////////////////////////////////////////////////////////////////////////////////
bool menuNode::changed(const navNode &nav,const menuOut& out,bool sub) {
  if (nav.target!=this) return dirty;
  if (dirty) return true;
  idx_t t=out.tops[nav.root->level];
  if (sub) for(int i=0;i<out.maxY();i++,t++) {
    if (t>=nav.sz()) break;
    if (operator[](t).changed(nav,out,false)) return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// navTarget - any menu object that can process keys/input
//
////////////////////////////////////////////////////////////////////////////////

void navTarget::doNav(navNode& nav,navCmd cmd) {
  nav.doNavigation(cmd);
}

void navTarget::parseInput(navNode& nav,Stream& in) {
  doNav(nav,nav.navKeys(in.read()));
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// navNode - Navigation layer controller
//
////////////////////////////////////////////////////////////////////////////////

//aux function, turn input character into navigation command
navCmd navNode::navKeys(char ch) {
  if (strchr(numericChars,ch)) {
    return navCmd(idxCmd,ch-'1');
  }
  for(uint8_t i=0;i<sizeof(options->navCodes)/sizeof(navCode);i++)
    if (options->navCodes[i].ch==ch) return options->navCodes[i].cmd;
  return noCmd;
}

// generic navigation (aux function)
navCmd navNode::doNavigation(navCmd cmd) {
  idx_t osel=sel;
  idx_t nsel=sel;
  navCmd rCmd=cmd;
  switch(cmd.cmd) {
    /*case scrlUpCmd:
      if (!target->isVariant())
        root->out.doNav(cmd,*this);*/
    case downCmd:
      nsel--;
      if (nsel<0) {if(wrap()) nsel=sz()-1; else nsel=0;}
      break;
    /*case scrlDownCmd:
      if (!target->isVariant())
        root->out.doNav(cmd,*this);*/
    case upCmd:
      nsel++;
      if (nsel>=sz()) {if(wrap()) nsel=0; else nsel=sz()-1;}
      break;
    case escCmd:
      assert(root);
      rCmd=root->exit();
      break;
    case enterCmd:
      assert(root);
      rCmd=root->enter();
      break;
    case selCmd:
    case idxCmd: {
        idx_t at=(idx_t)cmd.param;//-'1';send us numeric index pls!
        if (at>=0&&at<=sz()-1) {
          nsel=at;
          //if (cmd.cmd==idxCmd) {
            //Serial<<"indexing... "<<endl;
            //rCmd=root->enter();//this enter must occour latter wrapped in events, because selection changed
          //}
        }
      }
      break;
    case noCmd:
    default: break;
  }
  if(osel!=nsel||cmd.cmd==selCmd||cmd.cmd==idxCmd) {//selection changed, must have been and idx/sel or an up/down movement
    if (target->sysStyles()&(_parentDraw|_isVariant)) {
      target->dirty=true;
    } else {
      operator[](osel).dirty=true;
      operator[](nsel).dirty=true;
    }
    //send focus In/Out events
    if (selBlurEvent&target->events()) target->operator()(selBlurEvent,*target);
    event(blurEvent,osel);
    sel=nsel;
    if (cmd.cmd==selCmd||cmd.cmd==idxCmd) {//do accelerator and enter the option
      //Serial<<"index accel. "<<cmd<<(cmd.cmd&(selCmd|idxCmd))<<endl;
      assert(root);
      rCmd=root->enter();
    }//other commands up/down just receive focus events
    event(focusEvent,nsel);
    if (selFocusEvent&target->events()) target->operator()(selFocusEvent,*target);
  } //else its an enter/esc or a non-changing index!
  //Serial<<"doNavigation returning "<<rCmd<<endl;
  return rCmd;
}

result navNode::event(eventMask e,idx_t i) {
  prompt& p=operator[](i);
  eventMask m=p.events();
  eventMask me=(eventMask)(e&m);
  if (me) {
    return p(e,p);
  }
  return proceed;
}

result navNode::sysEvent(eventMask e,idx_t i) {
  prompt& p=operator[](i);
  return p(e,p);
}

void navRoot::doInput(Stream& in) {
  if (sleepTask) {
    if (options->getCmdChar(enterCmd)==in.read()) idleOff();
  } else {
    idx_t inputBurstCnt=options->inputBurst+1;
    //if (in.available())
    while ((!sleepTask)&&in.available()&&(--inputBurstCnt)) {//if not doing something else and there is input
      //Serial.print(".");
      navFocus->parseInput(node(),in);//deliver navigation input task to target...
    }
  }
}

void navRoot::doNav(navCmd cmd) {
  if (sleepTask&&cmd.cmd==enterCmd) idleOff();
  else if (!sleepTask) switch (cmd.cmd) {
    case scrlUpCmd:
    case scrlDownCmd:
      out.doNav(cmd,node());//scroll is perceived better at output device
      break;
    default:
      navFocus->doNav(node(),cmd);
  }
}


result maxDepthError(menuOut& o,idleEvent e) {
  o.print(F("Error: maxDepth reached!\n\rincrease maxDepth on your scketch."));
  return proceed;
}

navCmd navRoot::enter() {
  if (
    selected().enabled
    &&selected().sysHandler(activateEvent,node(),selected())==proceed
  ) {
    prompt& sel=selected();
    bool canNav=sel.canNav();
    bool isMenu=sel.isMenu();
    result go=node().event(enterEvent);//item event sent here
    navCmd rCmd=enterCmd;
    if (go==proceed&&isMenu&&canNav) {
      if (level<maxDepth) {
        active().dirty=true;
        menuNode* dest=(menuNode*)&selected();
        level++;
        node().target=dest;
        node().sel=0;
        active().dirty=true;
        sel.sysHandler(enterEvent,node(),selected());
        rCmd=enterCmd;
      } else {
        idleOn(maxDepthError);
        rCmd=noCmd;
      }
    } else if (go==quit&&!selected().isMenu()) exit();
    if (canNav) {
      navFocus=(navTarget*)&sel;
      navFocus->dirty=true;
    }
    return rCmd;
  }
  return noCmd;
}

navCmd navRoot::exit() {
  navFocus->dirty=true;
  if (navFocus->isMenu()) {
    if (level) {
      level--;
      node().event(exitEvent,node().sel);
    } else if (options->canExit) {
      node().event(exitEvent,node().sel);
      idleOn(idleTask);
    }
  } else node().event(exitEvent,node().sel);
  active().dirty=true;
  navFocus=&active();
  return escCmd;
}

bool fieldBase::async(const char *uri,navRoot& root,idx_t lvl) {
  if ((!*uri)||(uri[0]=='/'&&!uri[1])) return true;
  else if (uri[0]=='/') {
    StringStream i(++uri);
    parseInput(root.node(), i);
    return true;
  }
  return true;
}

void fieldBase::doNav(navNode& nav,navCmd cmd) {
  switch(cmd.cmd) {
    //by default esc and enter cmds do the same by changing the value
    //it might be set by numeric parsing when allowed
    case idxCmd: //Serial<<"menuField::doNav with idxCmd"<<endl;
    case escCmd:
      tunning=true;//prepare for exit
    case enterCmd:
      if (tunning||options->nav2D||!canTune()) {//then exit edition
        tunning=false;
        dirty=true;
        constrainField();
        nav.event(options->useUpdateEvent?updateEvent:enterEvent);
        nav.root->exit();
        return;
      } else tunning=true;
      dirty=true;
      break;
    case upCmd:
      stepit(1);
      break;
    case downCmd:
      stepit(-1);
      break;
    default:break;
  }
  /*if (ch==options->getCmdChar(enterCmd)&&!tunning) {
    nav.event(enterEvent);
  }*/
  if (dirty)//sending enter or update event
    nav.event(options->useUpdateEvent?updateEvent:enterEvent);
}

idx_t fieldBase::printTo(navRoot &root,bool sel,menuOut& out, idx_t idx,idx_t len,idx_t panelNr) {
  idx_t l=prompt::printTo(root,sel,out,idx,len,panelNr);
  bool ed=this==root.navFocus;
  //bool sel=nav.sel==i;
  if (l<len) {
    out.print((root.navFocus==this&&sel)?(tunning?'>':':'):' ');
    l++;
    if (l<len) {
      out.fmtStart(menuOut::fmtField,root.node(),idx);
      out.setColor(valColor,sel,enabled,ed);
      //out<<reflex;
      l+=printReflex(out);//NOTE: this can exceed the limits!
      out.fmtEnd(menuOut::fmtField,root.node(),idx);
      if (l<len) {
        out.fmtStart(menuOut::fmtUnit,root.node(),idx);
        out.setColor(unitColor,sel,enabled,ed);
        l+=print_P(out,units(),len);
        out.fmtEnd(menuOut::fmtUnit,root.node(),idx);
      }
    }
  }
  return l;
}

/////////////////////////////////////////////////////////////////
idx_t menuVariantBase::printTo(navRoot &root,bool sel,menuOut& out, idx_t idx,idx_t len,idx_t panelNr) {
  idx_t l=len;
  l-=prompt::printTo(root,sel,out,idx,len,panelNr);
  idx_t at=sync(sync());
  bool ed=this==root.navFocus;
  //bool sel=nav.sel==i;
  if (len-l<0) return 0;
  out.print(this==&root.active()?':':' ');
  l--;
  if (out.fmtStart(type()==selectClass?menuOut::fmtSelect:menuOut::fmtChoose,root.node(),idx)==proceed) {
    out.setColor(valColor,sel,prompt::enabled,ed);
    if (l>0) l-=operator[](at).printRaw(out,l);
  }
  out.fmtEnd(type()==selectClass?menuOut::fmtSelect:menuOut::fmtChoose,root.node(),idx);
  return len-l;
}

idx_t menuVariantBase::togglePrintTo(navRoot &root,bool sel,menuOut& out, idx_t idx,idx_t len,idx_t panelNr) {
  idx_t l=prompt::printTo(root,sel,out,idx,len,panelNr);
  idx_t at=sync(sync());
  bool ed=this==root.navFocus;
  //bool sel=nav.sel==i;
  out.setColor(valColor,sel,prompt::enabled,ed);
  //out<<menuNode::operator[](at);
  if (len-l>0) {
    out.fmtStart(menuOut::fmtToggle,root.node(),idx);
    l+=operator[](at).printRaw(out,len-l);
    out.fmtEnd(menuOut::fmtToggle,root.node(),idx);
  }
  return l;
}

void menuVariantBase::doNav(navNode& nav,navCmd cmd) {
  nav.sel=sync();
  navCmd c=nav.doNavigation(cmd);
  sync(nav.sel);
  if (c.cmd==enterCmd) {
    nav.root->exit();
  }
}
