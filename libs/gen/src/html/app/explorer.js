(function(){
"use strict";
var M=null, T=null, sel=null;
// Our own trail: following a member takes you off the class you were reading.
var trail=[];

// Named, not literal, so the stylesheet decides what each is in the current theme.
var KIND_COLOR={classes:"var(--kind-classes)",structures:"var(--kind-structures)",
  enumerations:"var(--kind-enumerations)",sequences:"var(--kind-sequences)",
  variants:"var(--kind-variants)",quantities:"var(--kind-quantities)",
  aliases:"var(--kind-aliases)",optionals:"var(--kind-optionals)"};
// The names come with the model, so a new kind is named in one place.
function kindLabel(k){var n=M.meta.kinds&&M.meta.kinds[k];return n?n.one:k;}
function kindPlural(k){var n=M.meta.kinds&&M.meta.kinds[k];return n?n.many:k;}
// The square before a name is its package. Kind is already said by the group and the
// chips, so it needs no colour of its own.
var PKG_PALETTE=["var(--pkg-0)","var(--pkg-1)","var(--pkg-2)","var(--pkg-3)",
  "var(--pkg-4)","var(--pkg-5)","var(--pkg-6)","var(--pkg-7)"];
// Keyed by names the model supplies, so none of these may inherit from Object.prototype:
// a package called "constructor" would read as a value that is already there.
function names(){return Object.create(null);}
var PKG_COLOR=names();
function assignPackageColours(names){
  names.slice().sort().forEach(function(p,i){
    PKG_COLOR[p]=p==="built-in"?"var(--pkg-builtin)":PKG_PALETTE[i%PKG_PALETTE.length];});
}
function pkgOf(n){var i=n.indexOf(".");return i<0?"":n.slice(0,i);}

var FLAG_CLASS={"Read Only":"f-ro","Writable":"f-rw","Dynamic":"f-dyn","Static":"f-stat",
  "Confirmed":"f-conf","Best Effort":"f-be"};

function el(t,cls,txt){var e=document.createElement(t);if(cls)e.className=cls;
  if(txt!=null)e.textContent=txt;return e;}
function ref(name){
  if(!T[name]) return el("span",null,name);
  var a=el("a","tref"); a.href="#"+name; a.dataset.n=name;
  var kd=el("span","kd"); kd.style.background=PKG_COLOR[T[name].package]||"var(--dim)";
  kd.title=(T[name].package||"built-in")+" · "+(kindLabel(T[name].kind));
  a.appendChild(kd);
  a.appendChild(document.createTextNode(name));
  return a;
}

/* The type as the STL that would declare it. The member table shows the same facts a
   row at a time; this shows the shape in one piece. */
function declaration(t){
  var pre=el("pre","decl"),k=t.kind,facts=t.facts||{};
  function put(cls,txt){pre.appendChild(el("span",cls,txt));}
  function tref(n){ if(T[n]){pre.appendChild(ref(n));} else put("ty",n||"?"); }
  function local(n){ return n&&n.indexOf(".")>=0?n.split(".").slice(1).join("."):n; }
  // No prose: the declaration carries the shape, the tables below carry the meaning.

  if(k==="builtins"){ put("cm","// built into sen\n"); put("ty",t.name); return pre; }

  if(k==="classes"){
    put("kw","class "); put("fld",local(t.name));
    if(t.ancestry.length){put("pun",": "); put("kw","extends "); tref(t.ancestry[t.ancestry.length-1]);}
    put("pun","\n{\n");
    var own=t.members||[];
    if(!own.length&&!(t.methods||[]).length&&!(t.events||[]).length)
      put("cm","  // nothing declared here; "+t.inherited+" inherited\n");
    own.forEach(function(m){
      put("pun","  "); put("kw","var "); put("fld",m.name); put("pun"," : "); tref(m.type);
      // Only what is not the default; no STL file writes the defaults out.
      var attrs=(m.flags||[]).filter(function(x){
        return x!=="Read Only"&&x!=="Best Effort"&&x!=="Dynamic";})
        .map(function(x){return x.toLowerCase();});
      if(attrs.length){put("pun"," ["); put("ty",attrs.join(", ")); put("pun","]");}
      put("pun",";\n");
    });
    (t.methods||[]).forEach(function(m){
      put("pun","  "); put("kw","fn "); put("fld",m.name); put("pun","(");
      (m.args||[]).forEach(function(a,i){ if(i)put("pun",", ");
        put("fld",a.name); put("pun"," : "); tref(a.type);});
      put("pun",")");
      if(m.returns&&m.returns!=="void"){put("pun"," -> "); tref(m.returns);}
      put("pun",";\n");
    });
    (t.events||[]).forEach(function(e){
      put("pun","  "); put("kw","event "); put("fld",e.name); put("pun","(");
      (e.args||[]).forEach(function(a,i){ if(i)put("pun",", ");
        put("fld",a.name); put("pun"," : "); tref(a.type);});
      put("pun",");\n");
    });
    put("pun","}");
    return pre;
  }

  if(k==="structures"){
    put("kw","struct "); put("fld",local(t.name));
    if(t.ancestry.length){put("pun",": "); tref(t.ancestry[t.ancestry.length-1]);}
    put("pun","\n{\n");
    var fs=t.members||[];
    fs.forEach(function(m,i){
      put("pun","  "); put("fld",m.name); put("pun"," : "); tref(m.type);
      if(i<fs.length-1)put("pun",",");
      put("pun","\n");
    });
    put("pun","}"); return pre;
  }

  if(k==="enumerations"){
    put("kw","enum "); put("fld",local(t.name));
    if(facts.representation){put("pun"," : "); tref(facts.representation);}
    put("pun"," { ");
    var rows=(t.table&&t.table.rows)||[];
    rows.slice(0,10).forEach(function(r,i){
      if(i)put("pun",", "); put("fld",r[0]);
      if(r[1]&&r[1]!==String(i))put("pun"," = "),put("ty",r[1]);
    });
    if(rows.length>10){put("pun",", "); put("cm","/* "+(rows.length-10)+" more */");}
    put("pun"," }"); return pre;
  }

  if(k==="variants"){
    put("kw","variant "); put("fld",local(t.name)); put("pun","\n{\n");
    var alts=(t.table&&t.table.rows)||[];
    alts.forEach(function(r,i){
      put("pun","  "); tref(r[0]); if(i<alts.length-1)put("pun",",");
      put("pun","\n");
    });
    put("pun","}"); return pre;
  }

  if(k==="sequences"){
    put("kw",facts.fixedSize?"array":"sequence"); put("pun","<"); tref(facts.element);
    if(facts.maxSize!==undefined){put("pun",", "); put("ty",String(facts.maxSize));}
    put("pun","> "); put("fld",local(t.name)); put("pun",";"); return pre;
  }

  if(k==="optionals"){
    put("kw","optional"); put("pun","<"); tref(facts.optional); put("pun","> ");
    put("fld",local(t.name)); put("pun",";"); return pre;
  }

  if(k==="aliases"){
    put("kw","alias "); put("fld",local(t.name)); put("pun"," "); tref(facts.aliased);
    put("pun",";"); return pre;
  }

  if(k==="quantities"){
    put("kw","quantity"); put("pun","<"); tref(facts.representation);
    if(facts.unit){put("pun",", "); put("ty",facts.unit);}
    put("pun","> "); put("fld",local(t.name)); put("pun",";");
    var lim=[];
    if(facts.min!==undefined)lim.push("min "+facts.min);
    if(facts.max!==undefined)lim.push("max "+facts.max);
    if(lim.length)put("cm","  // "+lim.join(", "));
    return pre;
  }

  put("ty",t.name); return pre;
}

/* ---------- tree ---------- */
function buildTree(){
  var s=document.getElementById("scroller"); s.textContent="";
  var kids=names(),roots=[];
  Object.keys(T).forEach(function(n){
    var t=T[n]; if(t.kind!=="classes") return;
    var p=t.ancestry.length?t.ancestry[t.ancestry.length-1]:null;
    if(p&&T[p]){(kids[p]=kids[p]||[]).push(n);} else {roots.push(n);}
  });
  function node(n){
    var t=T[n],li=el("li","t");
    var row=el("div","row"); row.dataset.n=n;
    var tw=el("span","tw"+((kids[n]&&kids[n].length)?"":" leaf"));
    var dot=el("span","dot"); dot.style.background=PKG_COLOR[t.package]||"var(--dim)";
    dot.title=t.package||"built-in";
    var nm=el("span","nm");
    var dotIdx=n.indexOf(".");
    nm.appendChild(el("span","pkg",n.slice(0,dotIdx+1)));
    nm.appendChild(document.createTextNode(n.slice(dotIdx+1)));
    row.append(tw,dot,nm);
    if(t.total) row.appendChild(el("span","badge",t.total+""));
    li.appendChild(row);
    if(kids[n]&&kids[n].length){
      var ul=el("ul","t"); ul.style.display="none";
      kids[n].sort().forEach(function(c){ul.appendChild(node(c));});
      li.appendChild(ul);
      tw.addEventListener("click",function(e){
        e.stopPropagation();
        var open=ul.style.display==="none";
        ul.style.display=open?"":"none"; tw.classList.toggle("open",open);
      });
    }
    return li;
  }
  var d=el("details","grp"); d.open=true; d.dataset.grp="classes";
  var sm=el("summary",null,"Classes "); sm.appendChild(el("span","n","("+roots.reduce(function(a,r){
    return a+1+(function cnt(x){return (kids[x]||[]).reduce(function(b,c){return b+1+cnt(c);},0);})(r);},0)+")"));
  d.appendChild(sm);
  var ul=el("ul","t root");
  roots.sort().forEach(function(r){ul.appendChild(node(r));});
  d.appendChild(ul); s.appendChild(d);

  var byKind=names();
  Object.keys(T).forEach(function(n){var t=T[n]; if(t.kind==="classes")return;
    (byKind[t.kind]=byKind[t.kind]||[]).push(n);});
  Object.keys(byKind).sort().forEach(function(k){
    var dd=el("details","grp"); dd.dataset.grp="data"; dd.dataset.kind=k;
    var s2=el("summary",null,kindPlural(k)+" ");
    s2.appendChild(el("span","n","("+byKind[k].length+")")); dd.appendChild(s2);
    var u=el("ul","t root");
    byKind[k].sort().forEach(function(n){
      var li=el("li","t"),row=el("div","row"); row.dataset.n=n;
      row.appendChild(el("span","tw leaf"));
      var dot=el("span","dot"); dot.style.background=PKG_COLOR[T[n].package]||"var(--dim)";
      dot.title=T[n].package||"built-in"; row.appendChild(dot);
      var nm=el("span","nm"); var i=n.indexOf(".");
      nm.appendChild(el("span","pkg",n.slice(0,i+1)));
      nm.appendChild(document.createTextNode(n.slice(i+1)));
      row.appendChild(nm); li.appendChild(row); u.appendChild(li);
    });
    dd.appendChild(u); s.appendChild(dd);
  });
}

/* ---------- filter ---------- */
// Classes first, because that is where people start. Data types are reached by
// following a member.
var kindMode="classes";
var offPkg=names(), offKind=names(), pkgCounts=names();

// Everything searchable, flattened once at boot. There is no site search behind this, so a
// description or a field name has to be findable too. Scored by a linear pass per keystroke.
var INDEX=[];
function buildIndex(){
  INDEX=Object.keys(T).map(function(n){
    var t=T[n],fields=[];
    fields.push(["name",n,100]);
    if(t.desc)fields.push(["description",t.desc,8]);
    (t.members||[]).forEach(function(m){
      fields.push(["field "+m.name,m.name,26]);
      if(m.desc)fields.push(["field "+m.name,m.desc,6]);});
    (t.groups||[]).forEach(function(g){g.members.forEach(function(m){
      fields.push(["inherited "+m.name,m.name,14]);});});
    ((t.table&&t.table.rows)||[]).slice(0,400).forEach(function(r){
      if(r[0])fields.push(["value "+r[0],r[0],12]);});
    (t.methods||[]).forEach(function(m){fields.push(["method "+m.name,m.name,26]);});
    (t.events||[]).forEach(function(e){fields.push(["event "+e.name,e.name,26]);});
    return {n:n,f:fields.map(function(x){return [x[0],x[1],x[2],x[1].toLowerCase()];})};
  });
}

function search(q){
  var out=[];
  for(var i=0;i<INDEX.length;i++){
    var e=INDEX[i],t=T[e.n];
    if(offPkg[t.package])continue;
    var best=0,why=null,label=null;
    for(var j=0;j<e.f.length;j++){
      var fld=e.f[j],at=fld[3].indexOf(q);
      if(at<0)continue;
      var s=fld[2];
      if(fld[3]===q)s*=3; else if(at===0)s*=2;
      else if(fld[0]==="name"&&fld[3].split(".").pop().indexOf(q)===0)s*=2;
      if(s>best){best=s;why=fld;label=fld[0];}
    }
    if(best)out.push({n:e.n,score:best,why:why,label:label});
  }
  out.sort(function(a,b){return b.score-a.score||a.n.localeCompare(b.n);});
  return out;
}

function snippet(text,q){
  var at=text.toLowerCase().indexOf(q);
  if(at<0)return text.slice(0,110);
  var from=Math.max(0,at-38);
  return (from?"…":"")+text.slice(from,at+q.length+72)+
         (at+q.length+72<text.length?"…":"");
}

// Chip counts track the tab; a package with nothing on it is marked empty rather than
// silently offering zero rows.
function refreshPkgCounts(){
  var host=document.getElementById("pkgchips"); if(!host) return;
  Array.prototype.forEach.call(host.children,function(b){
    var c=pkgCounts[b.dataset.v]||{classes:0,data:0};
    var n=kindMode==="classes"?c.classes:c.data;
    var span=b.querySelector(".c"); if(span)span.textContent=String(n);
    // A package with nothing on this tab is a dead option, not a filter.
    b.hidden=(n===0);
  });
}

function applyFilter(){
  var q=document.getElementById("q").value.trim().toLowerCase();
  var shown=0;
  var tree=document.getElementById("scroller"),res=document.getElementById("results");

  // A ranked list saying where each hit matched. A name match and a description match
  // are not the same thing.
  if(q){
    tree.hidden=true; res.hidden=false; res.textContent="";
    var hits=search(q);
    if(!hits.length){
      res.appendChild(el("div","nohits","Nothing matches \u201c"+q+"\u201d"));
    }
    hits.slice(0,300).forEach(function(hit){
      var t=T[hit.n],row=el("div","hit"); row.dataset.n=hit.n;
      var top=el("div","top");
      var dot=el("span","dot"); dot.style.background=PKG_COLOR[t.package]||"var(--dim)";
      dot.title=t.package;
      top.append(dot,el("span","nm2",hit.n),
                 el("span","where",hit.label==="name"?kindLabel(t.kind):hit.label));
      row.appendChild(top);
      if(hit.why&&hit.label!=="name"){
        var sn=el("div","sn"),text=snippet(hit.why[1],q),at=text.toLowerCase().indexOf(q);
        if(at<0)sn.textContent=text;
        else{sn.appendChild(document.createTextNode(text.slice(0,at)));
             sn.appendChild(el("mark",null,text.substr(at,q.length)));
             sn.appendChild(document.createTextNode(text.slice(at+q.length)));}
        row.appendChild(sn);
      }
      res.appendChild(row);
    });
    document.getElementById("hsub").textContent=hits.length+" matching";
    return;
  }
  tree.hidden=false; res.hidden=true;

  document.querySelectorAll("#scroller > details.grp").forEach(function(g){
    var isClasses=g.dataset.grp==="classes";
    var on=(kindMode==="classes")===isClasses;
    if(on&&!isClasses&&offKind[g.dataset.kind])on=false;
    g.style.display=on?"":"none";
  });

  document.querySelectorAll("#scroller li.t").forEach(function(li){
    var row=li.querySelector(":scope > .row"); if(!row) return;
    var n=row.dataset.n,t=T[n];
    var hit=!offPkg[t.package]
            &&((kindMode==="classes")===(t.kind==="classes"))
            &&(kindMode==="classes"||!offKind[t.kind]);
    li.classList.toggle("hide",!hit);
    if(hit){shown++;
      var p=li.parentElement;
      while(p&&p.id!=="scroller"){
        if(p.tagName==="UL"&&p.style.display==="none"){
          p.style.display="";
          var tw=p.parentElement.querySelector(":scope > .row > .tw");
          if(tw)tw.classList.add("open");
        }
        if(p.tagName==="DETAILS")p.open=true;
        if(p.tagName==="LI")p.classList.remove("hide");
        p=p.parentElement;
      }
    }
  });
  var filtering=Object.keys(offPkg).length>0||
                (kindMode==="data"&&Object.keys(offKind).length>0);
  document.getElementById("hsub").textContent=
    filtering?shown+" matching":M.meta.counts.types+" types";
}

/* ---------- detail ---------- */
/* A member occupies its own tbody: name and type on the first row, qualifiers and
   description beneath across the full width. */
function memberRows(m,origin,cols){
  var tb=el("tbody","member");
  var flags=(m.flags||[]).filter(Boolean);

  // Name, type and qualifiers read as one phrase, the way the declaration writes it.
  var tr=el("tr");
  var c1=el("td","n");
  c1.appendChild(el("span","mname",m.name));
  if(m.type){
    c1.appendChild(el("span","sep"," : "));
    c1.appendChild(ref(m.type));
  }
  if(flags.length){
    var fl=el("span","flags");
    flags.forEach(function(f){fl.appendChild(el("span","f "+(FLAG_CLASS[f]||""),f));});
    c1.appendChild(fl);
  }
  tr.appendChild(c1);
  if(origin!==undefined){
    var cf=el("td","ty");
    if(origin)cf.appendChild(ref(origin)); else cf.appendChild(el("span","self","itself"));
    tr.appendChild(cf);
  }
  tb.appendChild(tr);

  if(m.desc){
    var tr2=el("tr","detailrow");
    var cell=el("td","detailcell");
    if(cols>1)cell.setAttribute("colspan",String(cols));
    cell.appendChild(el("div","mdesc",m.desc));
    tr2.appendChild(cell); tb.appendChild(tr2);
  }
  return tb;
}

function membersTable(list,origins){
  var wrap=el("div");
  var tb=el("table","members"),th=el("tr");
  var cols=origins?["","From"]:[""];
  if(origins){
    cols.forEach(function(h){th.appendChild(el("th",null,h));});
    tb.appendChild(el("thead")).appendChild(th);
  }
  list.forEach(function(m,i){
    tb.appendChild(memberRows(m,origins?origins[i]:undefined,cols.length));
  });
  wrap.appendChild(tb); return wrap;
}

/* One table for every group, with the declaring class as a full-width row between them,
   so every group shares a column grid. */
function groupedMembersTable(sections){
  var total=sections.reduce(function(a,s){return a+s.members.length;},0);
  var wrap=el("div");
  var tb=el("table","members grouped");

  sections.forEach(function(s,gi){
    // A blank row before each class after the first, so the bands do not stack up.
    if(gi){
      var gap=el("tbody","gap");
      var gr=el("tr"); var gc=el("td");
      gr.appendChild(gc); gap.appendChild(gr); tb.appendChild(gap);
    }
    var head=el("tbody","groupband"); head.dataset.g=String(gi);
    var hr=el("tr","grouprow");
    var cell=el("td");
    cell.appendChild(el("span","caret"));
    cell.appendChild(el("span","from",s.title));
    cell.appendChild(el("span","n",String(s.members.length)));
    hr.appendChild(cell); head.appendChild(hr); tb.appendChild(head);

    s.members.forEach(function(m){
      var body=memberRows(m,undefined,1);
      body.dataset.g=String(gi);
      tb.appendChild(body);
    });
  });

  tb.addEventListener("click",function(e){
    var hr2=e.target.closest("tr.grouprow"); if(!hr2)return;
    var band=hr2.parentElement,g=band.dataset.g;
    var shut=band.classList.contains("shut");
    band.classList.toggle("shut",!shut);
    Array.prototype.forEach.call(tb.querySelectorAll("tbody.member"),function(b){
      if(b.dataset.g===g)b.classList.toggle("gone",!shut);});
  });

  wrap.appendChild(tb); return wrap;
}

/* The values a type carries that are not members: a variant's alternatives, an
   enumeration's enumerators. */
/* What defines a type that has no members, written as a sentence so the label and its
   type read as one fact. */
function definitionLine(t){
  var facts=t.facts||{},p=el("p","definition");
  function say(s){p.appendChild(document.createTextNode(s));}
  function typ(n){ if(T[n])p.appendChild(ref(n)); else p.appendChild(el("code",null,n)); }

  if(t.kind==="aliases"&&facts.aliased){ say("Alias for "); typ(facts.aliased); say("."); }
  else if(t.kind==="optionals"&&facts.optional){
    say("An optional "); typ(facts.optional); say(" \u2014 the value may be absent.");
  }
  else if(t.kind==="sequences"&&facts.element){
    if(facts.fixedSize){ say("A fixed array of "+(facts.maxSize!==undefined?facts.maxSize+" ":"")); typ(facts.element); say("."); }
    else if(facts.bounded){ say("A sequence of "); typ(facts.element);
      say(facts.maxSize!==undefined?", at most "+facts.maxSize+".":", bounded."); }
    else { say("An unbounded sequence of "); typ(facts.element); say("."); }
  }
  else if(t.kind==="quantities"){
    say("Stored as "); typ(facts.representation||"?");
    if(facts.unit)say(", measured in "+facts.unit);
    say(".");
    if(facts.min!==undefined||facts.max!==undefined){
      var parts=[];
      if(facts.min!==undefined)parts.push("no less than "+facts.min);
      if(facts.max!==undefined)parts.push("no more than "+facts.max);
      say(" Values are "+parts.join(" and ")+".");
    }
  }
  else if(facts.representation){ say("Stored as "); typ(facts.representation); say("."); }
  else return null;

  return p;
}

function plainTable(spec){
  var big=spec.rows.length>60;
  var wrap=el("div");
  // Shrink to the contents when the second column is short, such as an enumerator's
  // value, so the pair stays readable as a pair.
  var widest=spec.rows.reduce(function(a,r){
    return Math.max(a,String(r[r.length-1]||"").length);},0);
  var tb=el("table","values"+(widest<=24?" tight":""));
  if(spec.header.some(function(h){return h;})){
    var th=el("tr");
    spec.header.forEach(function(h){th.appendChild(el("th",null,h));});
    tb.appendChild(el("thead")).appendChild(th);
  }
  var body=el("tbody");

  function row(r){
    var tr=el("tr");
    r.forEach(function(c,i){
      var td;
      if(T[c]){ td=el("td","ty"); td.appendChild(ref(c)); }
      else if(i===0&&c.length<40&&r.length>1){ td=el("td","n",c); }
      else { td=el("td","vdesc",c); }
      tr.appendChild(td);
    });
    return tr;
  }

  var shown=big?spec.rows.slice(0,400):spec.rows;
  shown.forEach(function(r){body.appendChild(row(r));});
  tb.appendChild(body); wrap.appendChild(tb);

  if(big&&spec.rows.length>shown.length){
    var note=el("div","more");
    note.appendChild(document.createTextNode(
      "Showing "+shown.length+" of "+spec.rows.length+". "));
    var b=el("button",null,"Show all");
    b.onclick=function(){
      spec.rows.slice(shown.length).forEach(function(r){body.appendChild(row(r));});
      note.remove();
    };
    note.appendChild(b); wrap.appendChild(note);
  }
  return wrap;
}

/* ---------- hierarchy ---------- */
/* Where a type sits: the line of descent above it, what shares its parent, and what extends
   it. Interfaces are off, so there is at most one parent and the line is never a graph, which
   is what makes this a stack of boxes rather than a job for a layout engine. */
var childrenOf=null;
function buildChildIndex(){
  childrenOf=names();
  Object.keys(T).forEach(function(n){
    var a=T[n].ancestry;
    if(!a.length)return;
    var p=a[a.length-1];
    if(T[p])(childrenOf[p]=childrenOf[p]||[]).push(n);
  });
  Object.keys(childrenOf).forEach(function(k){childrenOf[k].sort();});
}

function hbox(name,current){
  var b=el("span","hbox"+(current?" here":""));
  var dot=el("span","kd"); dot.style.background=PKG_COLOR[T[name].package]||"var(--dim)";
  b.appendChild(dot);
  b.appendChild(document.createTextNode(name.split(".").pop()));
  if(current){ b.title=name; return b; }
  var a=el("a","hlink"); a.href="#"+name; a.dataset.n=name; a.title=name;
  a.appendChild(b); return a;
}

// Wrapped rather than scrolled: a scrollbar inside the pane hides how many there are.
function hrow(label,members){
  var wrap=el("div","hgroup");
  wrap.appendChild(el("div","hlabel",label+" ("+members.length+")"));
  var row=el("div","hrow");
  members.forEach(function(n){row.appendChild(hbox(n,false));});
  wrap.appendChild(row); return wrap;
}

function hierarchy(name){
  if(!childrenOf)buildChildIndex();
  var t=T[name], chain=t.ancestry.filter(function(a){return T[a];});
  var parent=chain.length?chain[chain.length-1]:null;
  var siblings=(parent&&childrenOf[parent]||[]).filter(function(n){return n!==name;});
  var children=childrenOf[name]||[];
  if(!chain.length&&!siblings.length&&!children.length)return null;

  var box=el("div","hier");
  var line=el("div","hchain");
  chain.concat([name]).forEach(function(n,i){
    if(i)line.appendChild(el("div","hpipe"));
    line.appendChild(hbox(n,n===name));
  });
  box.appendChild(line);
  if(siblings.length)box.appendChild(hrow("Shares a parent with",siblings));
  if(children.length)box.appendChild(hrow("Extended by",children));
  return box;
}

function section(title,count,node){
  var s=el("section"),h=el("h3",null,title+" ");
  if(count!=null)h.appendChild(el("span","count","("+count+")"));
  s.appendChild(h); s.appendChild(node); return s;
}

// Nothing is selected until the reader selects it; which type comes first is their question.
function showNothing(){
  sel=null;
  var d=document.getElementById("detail"); d.textContent="";
  var box=el("div","nothing");
  box.appendChild(el("p","big",Object.keys(T).length
    ?"Select a type to inspect it."
    :"This model declares no types."));
  if(Object.keys(T).length){
    box.appendChild(el("p",null,"Pick one from the list, or search by name, field or description."));
  }
  d.appendChild(box);
}

function show(name,viaTrail){
  var t=T[name];
  if(!t){showNothing();return;}
  if(!viaTrail&&sel&&sel!==name)trail.push(sel);
  var b=document.getElementById("back");
  if(b){b.disabled=trail.length===0;
        b.title=trail.length?"Back to "+trail[trail.length-1]:"Back";}
  sel=name;
  var d=document.getElementById("detail"); d.textContent=""; d.scrollTop=0;

  if(t.ancestry.length){
    var c=el("div","crumb");
    t.ancestry.forEach(function(a){
      c.appendChild(ref(a)); c.appendChild(el("span","sep","›"));
    });
    c.appendChild(el("strong",null,name.split(".").pop()));
    d.appendChild(c);
  }
  var head=el("div","titlerow");
  head.appendChild(el("h2","tt",name));

  var chips=el("span","chips");
  chips.appendChild(el("span","chip k",kindLabel(t.kind)));
  if(t.package){
    var pchip=el("span","chip",t.package);
    pchip.style.borderColor=PKG_COLOR[t.package]||"var(--line)";
    pchip.style.color=PKG_COLOR[t.package]||"var(--dim)";
    chips.appendChild(pchip);
  }
  if(t.fanIn)chips.appendChild(el("span","chip","used by "+t.fanIn));
  head.appendChild(chips);
  d.appendChild(head);
  d.appendChild(el("hr","titlerule"));

  if(t.desc)d.appendChild(el("p","desc",t.desc));

  var h=hierarchy(name);
  if(h)d.appendChild(section("Hierarchy",null,h));

  // Anything that inherits, not only classes: a struct with a parent carries its fields too.
  if((t.members||[]).length||(t.groups||[]).length){
    var carried=t.kind==="classes"?"Properties":"Fields";
    var own=(t.members||[]).length;
    var all=own+(t.groups||[]).reduce(function(a,g){return a+g.members.length;},0);

    var wrap=el("div");
    var tools=el("div","tools");
    var lbl=el("label"); var cb=el("input"); cb.type="checkbox"; cb.checked=true;
    lbl.append(cb,document.createTextNode(" show inherited members"));
    tools.appendChild(lbl); wrap.appendChild(tools);
    var host=el("div"); wrap.appendChild(host);

    var sect=section(carried,all,wrap);
    var counter=sect.querySelector(".count");

    function draw(){
      host.textContent="";
      // Ticked: everything an instance carries, grouped by the declaring class.
      // Unticked: only what this class declares itself, which is often nothing.
      if(cb.checked){
        var sections=[];
        if(own)sections.push({title:t.name,members:t.members});
        (t.groups||[]).forEach(function(g){
          sections.push({title:g.from,members:g.members});});
        host.appendChild(groupedMembersTable(sections));
        if(counter)counter.textContent="("+all+")";
      } else if(own){
        host.appendChild(membersTable(t.members));
        if(counter)counter.textContent="("+own+")";
      } else {
        host.appendChild(el("p","empty","Nothing of its own; all "+all+" come from what it extends."));
        if(counter)counter.textContent="(0)";
      }
    }
    cb.addEventListener("change",draw); draw();
    d.appendChild(sect);
  }

  if((t.methods||[]).length){
    var mt=el("div");
    t.methods.forEach(function(m){
      var box=el("details","inh"); box.open=true;
      var sm=el("summary"); sm.appendChild(el("code",null,m.name));
      if(m.returns&&m.returns!=="void"){
        sm.appendChild(document.createTextNode(" \u2192 "));
        sm.appendChild(T[m.returns]?ref(m.returns):el("code",null,m.returns));
      }
      box.appendChild(sm);
      var inner=el("div");
      if(m.desc)inner.appendChild(el("p","desc",m.desc));
      if((m.args||[]).length)inner.appendChild(membersTable(m.args.map(function(a){
        return {name:a.name,type:a.type,desc:a.desc,flags:[]};})));
      box.appendChild(inner); mt.appendChild(box);
    });
    d.appendChild(section("Methods",t.methods.length,mt));
  }

  if((t.events||[]).length){
    var ev=el("div");
    t.events.forEach(function(e){
      var box=el("details","inh");
      var sm=el("summary"); sm.appendChild(el("code",null,e.name)); box.appendChild(sm);
      var inner=el("div");
      if(e.desc)inner.appendChild(el("p","desc",e.desc));
      if((e.args||[]).length)inner.appendChild(membersTable(e.args.map(function(a){
        return {name:a.name,type:a.type,desc:a.desc,flags:[]};})));
      box.appendChild(inner); ev.appendChild(box);
    });
    d.appendChild(section("Events",t.events.length,ev));
  }

  var defn=definitionLine(t);
  if(defn)d.appendChild(defn);

  if(t.table&&t.table.rows&&t.table.rows.length)
    d.appendChild(section(t.table.header[0]||"Values",t.table.rows.length,plainTable(t.table)));

  // Open by default. It sits below everything else, so a reader who does not write STL
  // has already read what they came for, and one who does gets it without a click.
  var decl=el("details","inh aside"); decl.open=true;
  var ds=el("summary",null,"How this is written in STL");
  decl.appendChild(ds);
  var dw=el("div"); dw.appendChild(declaration(t)); decl.appendChild(dw);
  d.appendChild(decl);

  if(t.usedBy&&t.usedBy.length){
    var u=el("div","usedby");
    t.usedBy.slice(0,60).forEach(function(n){u.appendChild(ref(n));});
    if(t.usedBy.length>60)u.appendChild(el("span","more","+"+(t.usedBy.length-60)+" more"));
    d.appendChild(section("Used by",t.usedBy.length,u));
  }

  document.querySelectorAll("#scroller .row").forEach(function(r){
    r.classList.toggle("on",r.dataset.n===name);});
  document.querySelectorAll("#results .hit").forEach(function(r){
    r.classList.toggle("on",r.dataset.n===name);});
  var on=document.querySelector("#scroller .row.on");
  if(on){var p=on.parentElement;
    while(p&&p.id!=="scroller"){
      if(p.tagName==="UL"&&p.style.display==="none"){
        p.style.display="";
        var tw=p.parentElement.querySelector(":scope > .row > .tw");
        if(tw)tw.classList.add("open");}
      if(p.tagName==="DETAILS")p.open=true;
      p=p.parentElement;}
    on.scrollIntoView({block:"nearest"});}
  if(location.hash.slice(1)!==name)history.pushState(null,"","#"+name);
}

/* ---------- boot ---------- */
// decodeURIComponent throws on a malformed escape such as "#%"; boot must survive one.
function fragment(){
  var raw=location.hash.slice(1);
  try{ return decodeURIComponent(raw); }catch(err){ return raw; }
}
// model.js runs before this and leaves the model on the window. A script tag rather
// than a fetch, because a fetch fails outright from a file path.
start(window.__senModel);

function start(data){
  if(!data){document.getElementById("detail").textContent=
    "model.js did not load."; return;}
  // Settled before the model is indexed, or the page repaints in the wrong theme for as long
  // as the model takes to load. The stylesheet follows the machine on its own; this applies
  // only a choice the reader made earlier.
  var root=document.documentElement, chosen=null;
  try{chosen=localStorage.getItem("sen.theme");}catch(err){}
  if(chosen)root.dataset.theme=chosen;
  M=data;
  // Prototype stripped: otherwise T["constructor"] is a function and reads as a type, so a
  // fragment or an enumerator carrying that name resolves to something that is not there.
  T=Object.assign(names(),data.types);
  // Each filter is a label you switch off. All on means no filtering.
  function chips(host,entries,state,colour){
    entries.forEach(function(e){
      var b=el("button"); b.dataset.v=e.value;
      if(colour){var kd=el("span","kd"); kd.style.background=colour(e.value); b.appendChild(kd);}
      b.appendChild(document.createTextNode(e.label));
      b.appendChild(el("span","c",String(e.count)));
      b.title="Click to hide "+e.label;
      b.addEventListener("click",function(){
        var off=!!state[e.value];
        if(off)delete state[e.value]; else state[e.value]=true;
        b.classList.toggle("off",!off);
        b.title=(off?"Click to hide ":"Click to show ")+e.label;
        applyFilter();
      });
      host.appendChild(b);
    });
  }
  // A package holds different amounts of each kind, so the count follows the tab or it
  // promises rows the current view cannot show.
  pkgCounts=names();
  Object.keys(T).forEach(function(n){
    var t=T[n]; if(!t.package)return;
    var c=pkgCounts[t.package]||(pkgCounts[t.package]={classes:0,data:0});
    if(t.kind==="classes")c.classes++; else c.data++;});
  assignPackageColours(Object.keys(pkgCounts));
  chips(document.getElementById("pkgchips"),
    Object.keys(pkgCounts).sort().map(function(p){
      return {value:p,label:p,count:0};}), offPkg,
    function(p){return PKG_COLOR[p]||"var(--dim)";});
  refreshPkgCounts();
  chips(document.getElementById("kindchips"),
    Object.keys(M.meta.counts.kinds).sort().filter(function(k){return k!=="classes";})
      .map(function(k){
        return {value:k,label:kindPlural(k),count:M.meta.counts.kinds[k]};}),
    offKind, function(k){return KIND_COLOR[k]||"var(--dim)";});

  buildIndex();
  buildTree();
  // The tree is built whole and the opening view is a filtered one, so apply it now.
  applyFilter();
  document.getElementById("hsub").textContent=M.meta.counts.types+" types";
  var f=document.getElementById("foot");
  var made=el("span","made");
  var mark=el("img"); mark.setAttribute("src","logo.svg"); mark.setAttribute("alt","");
  mark.className="logo";
  made.appendChild(mark);
  // RFC 3339, shown as date and minutes. Left in UTC: the reader's zone is not where it ran.
  var when=M.meta.generated||"";
  var shown=/^\d{4}-\d\d-\d\dT\d\d:\d\d/.test(when)?when.slice(0,10)+" "+when.slice(11,16)+" UTC":"";
  made.appendChild(document.createTextNode(
    shown?"Generated on "+shown+" by "+(M.meta.generator||"Sen")
         :"Generated by "+(M.meta.generator||"Sen")));
  f.appendChild(made);
  f.appendChild(el("span","gen",M.meta.counts.types+" types in "+
    Object.keys(M.meta.counts.packages).length+" packages"));
  var hint=el("span","hint");
  [["/","search"],["↑↓","move"],["↵","open"],["⌫","back"]].forEach(function(p){
    hint.appendChild(el("kbd",null,p[0])); hint.appendChild(document.createTextNode(p[1]));});
  f.appendChild(hint);

  document.getElementById("q").addEventListener("input",applyFilter);
  document.getElementById("seg").addEventListener("click",function(e){
    var b=e.target.closest("button[data-v]"); if(!b)return;
    kindMode=b.dataset.v;
    this.querySelectorAll("button").forEach(function(x){x.classList.toggle("on",x===b);});
    document.getElementById("kindchips").hidden=(kindMode!=="data");
    refreshPkgCounts();
    applyFilter();
  });
  document.addEventListener("click",function(e){
    var a=e.target.closest("a.tref"); if(a){e.preventDefault();show(a.dataset.n);return;}
    var r=e.target.closest("#scroller .row"); if(r&&!e.target.closest(".tw")){show(r.dataset.n);return;}
    var hit=e.target.closest("#results .hit"); if(hit)show(hit.dataset.n);
  });
  document.getElementById("back").onclick=function(){
    if(!trail.length)return; show(trail.pop(),true);
  };
  window.addEventListener("popstate",function(){
    var hh=fragment();
    if(T[hh])show(hh,true); else showNothing();
  });
  // / to search, arrows through what is visible, Enter to open, Esc to clear,
  // Backspace to go back.
  function visibleRows(){
    var res=document.getElementById("results");
    if(!res.hidden)return Array.prototype.slice.call(res.querySelectorAll(".hit"));
    return Array.prototype.filter.call(
      document.querySelectorAll("#scroller .row"),
      function(r){var li=r.closest("li.t");
        if(li&&li.classList.contains("hide"))return false;
        return r.offsetParent!==null;});
  }
  document.addEventListener("keydown",function(e){
    var q=document.getElementById("q");
    if(e.key==="/"&&e.target!==q){e.preventDefault();q.focus();q.select();return;}
    if(e.key==="Escape"&&e.target===q){q.value="";applyFilter();q.blur();return;}
    if((e.key==="Backspace"||(e.altKey&&e.key==="ArrowLeft"))&&e.target!==q){
      e.preventDefault();var b=document.getElementById("back");
      if(!b.disabled)b.click();return;}
    if(e.key!=="ArrowDown"&&e.key!=="ArrowUp"&&e.key!=="Enter")return;
    var rows=visibleRows(); if(!rows.length)return;
    var cur=rows.findIndex(function(r){return r.classList.contains("on");});
    if(e.key==="Enter"){ if(e.target===q&&rows.length){e.preventDefault();show(rows[0].dataset.n);} return; }
    e.preventDefault();
    var next=e.key==="ArrowDown"?Math.min(rows.length-1,cur+1):Math.max(0,cur-1);
    if(cur<0)next=0;
    show(rows[next].dataset.n);
  });

  // Draggable divider, remembered between sessions.
  (function(){
    var sp=document.getElementById("split"),saved=null;
    try{saved=localStorage.getItem("sen.treew");}catch(err){}
    if(saved)document.documentElement.style.setProperty("--treew",saved);
    sp.addEventListener("pointerdown",function(ev){
      ev.preventDefault(); sp.setPointerCapture(ev.pointerId);
      sp.classList.add("dragging"); document.body.classList.add("resizing");
      function move(e){
        var w=Math.min(Math.max(e.clientX,190),Math.min(760,window.innerWidth-320));
        document.documentElement.style.setProperty("--treew",w+"px");
      }
      function up(e){
        sp.releasePointerCapture(ev.pointerId);
        sp.classList.remove("dragging"); document.body.classList.remove("resizing");
        sp.removeEventListener("pointermove",move); sp.removeEventListener("pointerup",up);
        try{localStorage.setItem("sen.treew",
          getComputedStyle(document.documentElement).getPropertyValue("--treew").trim());}catch(err){}
      }
      sp.addEventListener("pointermove",move); sp.addEventListener("pointerup",up);
    });
    sp.addEventListener("dblclick",function(){
      document.documentElement.style.setProperty("--treew","320px");
      try{localStorage.removeItem("sen.treew");}catch(err){}
    });
  })();

  document.getElementById("theme").onclick=function(){
    root.dataset.theme=root.dataset.theme==="dark"?"light":"dark";
    try{localStorage.setItem("sen.theme",root.dataset.theme);}catch(err){}
  };

  var h=fragment();
  if(T[h])show(h); else showNothing();
}
})();
