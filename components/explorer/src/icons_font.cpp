// === icons_font.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "icons_font.h"

// imgui
#include "imgui.h"

// std
#include <cassert>
#include <cstring>

ImFont* addIconsFont()
{
  static const char iconsCompressedDataBase85[9675 + 1] =
    "7])#######<>$3:'/"
    "###O@?>#+lQS%Ql#v#X^@iFaQ[9^&_'##(t*##YVV=BCj^xO:^Ee-]br-$KQshF?[]Y#qw$##20cw']$r0FE&l:#gwVw02)[w'S@>RDQjUw#"
    "0VD_&8Jq-$Mu@['"
    "BoK*NLmF_&UKo-$fZIkE:4&E;#K*##1E&##@+C0FKR4[0L)(F7^do-$8.HkOQ&###U::d?odPirc[4gL)>$##+Yk>39=5F%I,Y:v3%%s$4e2W%"
    "0V`q2wgSe$K8>uu8t2W%P6w0#*Vk(N"
    "T$V*NBVkJM(vC^#=d8KMW=6##R9dA#a7jV7-PNh#v0[0#$4,F%$bj-$#=[0#aL@6/SQUV$U/(pL+%g+M7:#gLEe`=-?4`T.Y9[0#K*/"
    ">-ikEB-3.pGMhJ+kL%7K/Lavj-$$^N_&tufw'"
    "VcFv$Yv[;%&7vV%C-ew'h0Q#$iR;e.4Dc>#@O4U%g=6;-=K$)*'q9$+:kug94*('N,$NNST'lpZ#iQZbdd]mn$UF'*N@1k/"
    "g4;0KCrTZkRfCh'abc^j#),##&ht^*(wE)bbWAQng@iJ)"
    "%(%sI;xK'JMVtA#255L#dA9VQ-C4)#v1)mL0_k)#ghkP9#>B;-av26.UxefLNtf]$0u68%47no%8ONP&<h/"
    "2'@*gi'DBGJ(HZ(,)Ls_c)P5@D*TMw%+XfW]+](9>,a@pu,eXPV-%xthY"
    "B7En<^O)##$w%##J:$##l5k-$76+##e-%##'s+##Gm#50Hx###J**##q3n-$^[)##i)+##Jfl-$@^+6'#NJM'6(.m/#Xfi'_`+/"
    "(2q]G2cjFJ(8rbf(+'(,)V.CG)OVjfL7E`hLQd+>-"
    "4npIVg-PoJ_a.R3$wvE74])R<?6]0#n_ErL3uXt$Y/"
    "H9i_DD]-j2dJ(7R6V#>GZ)G;-Wc$`Fv(%_$j9`*-Te$RfUV$vN>n%=*#,2]V5D<G.H]F2[ZuP9<hc),39Z-TekD#7%x[-a&,[P"
    "[4CSf]KWu004br/"
    "CV4'5p:PS7@'.`&RmwjO<nZ=Pglp@P+.eaObY>;-Po<I$QbFwN*R_@O:&PSR.SLDONl$##&&>uuYdUV$cG.%#p,_'#w-Qq7ZL^&=OdDb3sJ))"
    "3x#Ch1S6j?#sO>;-"
    "l@@[#noL^#4T'?-h(Et(T((c'6Cs#l<N<T'jjX-RFinWtDw'Y#e$SYG:4)kuD+eEfU%]H#+7=*$Ss.F.Qn&pAN$6AuL&JjL%Zh[-:Z9]0Gn(9#%"
    "/5##]c68%w2###OOVmLwYX%$)3qB#"
    "U%`v#,RhhLn>+D#XnK,3oqp6*bJ=/"
    "M*8])*$4Q)4#Vpr6&fcs-j_OcMG]@C#RKX$09Of@#Wksi*U@`g_^>e`*7A+Hr:m[(sGn)9'x6UhLJTZIqT4<L)AC)?uWT,A#Zv5K)as>X(;P<X("
    "9^f*.L?a*.t%7P)GGIcV`K9>$/"
    "4K-)Xf?%bS8i0(c6n3)`re+M+u]+M&@%I47T3.)vtWaag$3Q'(c(C&n0(67T#556x`a`sY1vr$kP2>5Tjw.:[pY]4D/"
    "NF3fG;<%WC7a4iStSpD&+P("
    "7%x[-Di^F*N,`l-*BG0l*)4I)D_RmAu?lD##&AA4Fl:?#^F@8%o(b.3n`aI)VSpxSOR8##KceCWgH/"
    "@#II$JIW1]JY27fS7U<Tw#rB_R'`*a$u8w%?A@Yn;8/IWp'xm@;-b/T=l4_lcu"
    "xcPc<JKBA#p:^?.)qK`aid?Au'L:@9-j'8+#R:hLC=Ss$;;mw9R1pgLJ[A[#$_^UunPa^#ha@;-dNo?9mWLfLF=6##(F)w$(8[0#.i<<"
    "8CFS7js6O;R+RqkLWjA@#t(MB#1w;Q.wR;?#"
    "`x4Z$-ptN.C=7s$m$<:&Pd$H)HA1w*$O9]=DF4rLU'A8%5Wk*%&i7]#&^eRPu,aZ#DNM]uMr,9&Yj`.hq'=d*2UJU2V5O@tRQ^$9LPx'#)Er(v-"
    "qKb$tH.%#PT5qVAs6qVbp%ENh0/)*"
    "7kPFuBBW1)G8][#5cIE)^MYd32AVD)Qbt6*O2Pn*]/=X(Nt]0>^5###eXS/"
    "Lv7?8%OJW]+aP>c`dO=R*r.rv-0S<+3$P#<-9]GW-Q2D(&*ZWI)?7%s$nbK+*b^5N'CXX/24hvv$]AqB#"
    "+%I*R.O]s$ax;9/W=[x6Ckx<(e1TV6#@n=7cj#;/"
    "jSpG3Y]%@'JP,G4Rq@.*q%^F*KjE.3tc];%J++^,*#jE-;8mpLubP[.CFMs-er.RMU4emLLi?lL'ZkZ/K]B.*eo^I*w]u'8mApM("
    "Ys[;%2M3N6l(MBSOG]>hr`?ODq,97;7Go/1eNFq%VM*u-+3*E*nOB6&D>gT&^Ueh_X/"
    "ID*AQkD%+Bx;Q-Ha5(>7FE&Z;([-`vF[0swraO-$_m&V>^p&;?vR&MJ)<-G@f&'<%A5J2/TU'"
    "nVVO'IB-5J#Aen&m$nK(G#4s6#h^404=s;$9SmO(OYa>25O-.M^3@/vE:BO;#/"
    "t3;@4nb%<g&VQPj@@#c6_K21=gm&r5tD%_waaE.wg6&5<XX?(Rhk+=WB*<Ngad*w,N^OF1HZ,cP@p("
    "V`uEOIsCQ&q?^I2pdYJ(IS.>(-``@G5oui9E,mY#6]S'$/4Dt-5I1hL;[HG*9RC^#GMDpL&W[90`VWk+5Jol'Uf+94(m16/"
    "(2###R=;mLwSC:%sYc8/dj</MflfF4hoEb3VA@r%Qua)3"
    "F,lv6VHL,3)VFb3c%sP030DDN/*8.*.ke?urE]:/M1r7/"
    "%A@`a=E^[#b*1JYhvbF'>]_V$+>OS7ew,?8n0TG)J*k5&:qRU.6KDMTBP_lLp2oiL<0vN+/(`D+jYE_4@LGo7ea74'wBQC+"
    "(5aa#16_d#AnaT%.tXAYRlIfLu*B/Lw12T.hvAv-1E/"
    "C%'>iG*m+?m8kvY,*buL?75AkJkt729.IAV##MSu;-lLdh-*['wIlMVJY.&+#v0'JfL4dr'#X,>>#0k'u$K5O;0WEj?#`qi?#"
    "u^/I$pwkj1cN'>.7a'<-I/[W%&fcJ(=h*=$*7X1;$f`h3KeIrHuZS5/+^Qe2;pa(X2r,K1_6O5&Ou(<AP1SxkQ/Roe#^^gu$o/"
    "M#l-T9;X4uILUF08@w[0NE525K35b`]5]BjH*VDE/>"
    "lS+?80-CH#w+t'uY3lmug+<+rM####;ORwumZ&e$UNc##h>$(#g2^jMbB<?#HXX/2_V8f3%:OA#7/"
    "#d$b.`$#ae_F*Yb@W$:F9a#=uk,2Tb^d3P74`#Gx)#/tkw,EqE->YliG`j87(WP"
    ",cxX#H-eoKoX`ZuZqhSRs3%L(s&e8/e/Yt1Re$`.?uRt1W9KN#K@p/"
    "N)[(?#j&:W-$v.T&rkJ+$]aWhLAN-##XWAe$&wIfL>N?>#R-PJ(YR@$T0_A$Ts20@#?2'J3L1fPJf)2G))2p^+"
    "H'1,)f_bM(dkQ##Tk6I0P1i?u(6GX$J'(,)&FmV#)+jT/.i>%$om.VQ:e,/"
    "q2:CP87a&##oXnan@Dn;%ocWI)^?Nk1i%Ad)nM.)*t?(E#s6SY-#VJ:)eX4@$K)'J38a[D*E4UhLThF<%"
    "22Tj2TU$lL3*E.3rqIg)-[qhLId^$-KU`5,c<]Q0pPrS&HIbr/rxVo&-FI&,_-/q/B$b@ulPr8,#%x:RGSis+2oXg29+#ARgWGj=KOLD?laY/"
    ">;Yvx'YhTs/Ncj?-l_TB+V<.'5v6YgL"
    "T[8q/D#9D+hc4SnCie&,j;LS/[L/"
    "a+w=8N1Gf1w=.r,Yfl)Vg>Zd;e?6rX&#Q3n0#3k8e$jrn%#>*'u$4TY&#SGha4^RS(#ft5,McMihLc76`,80FKl9,U__u38JH'X<3hTCru5vh2@"
    "S"
    "xR<P#&]DV#D,#RjYP50h:(*mPXe;7;Rl=9#'g.^..cUV$3]vt$`Sm/;bk0XSPT'O0jH@E+4Ht7/"
    "B:YGM>IH<-]M`.M0XJrmq@?tLbDH%bp.tS78,f3O[5R]4kN%##.cLs-;va.3e<&H;"
    "shF&#[Ho4X9vB0Gic*i#Z1]E)$h)9'Z$]<$T?dBte%HV)kJdofXKmA#`W5W-w3o63=L1-M/"
    "Iaul;(lA#168>-4Z6>5%,GuuREjd$p@O&#A?T:%p;Tv-[P[]4r;Pb%1JXa4S]DD3LR(f)"
    "gC-p.m[=%$_8m>u6vB>$S6+=$Vt^F$]J.h(dt0a*:/"
    "c3)DCSP#$VL,)s,nb*aBA]#WH]w#hiZP&1,N>5n4+M(fNb(3q*EtCL&>uuZgUV$Be@S:5Be#6:H7g)6Y^=90x(<);)'J38f)Q9"
    "&v.731jic)9IkA,^U3B-oDqd%PB/"
    ">#vZH*&-tmJ;QRHL,Yt*T.eUT`3@<C[-[o>_d$,bta,(NZP'qgL;.`$.6kmEdaGBB75T.=GM`7U*%2(V$#S,>>#V>^0W<K.IMTg`[,aLVs-M$+"
    "mL"
    "x$1j%g9(,)MfbF']6=t$YCl0LMxI=lNTAE+cpo7/BfH=lew,?8`p&<Av-gfL.s24'W0-e30k&%b:Qi)=X?p7/X:Y<6:#+?8DZFZ-8[-LE/"
    "&:e$fYI%#urgo.+Oh8/7Ik`S%f]B,8&B.*"
    "44br/"
    "VF_L&=v7+0hXKb@@hsPM2.VfLEF8GV<cUr#j%D&5ld=?-vXGs-;,3cM(cR`ae0l>-YG?dbq()WHvXeP(:_$29XT^`3:5Rv$F^?d)iHL,3sc@A4#"
    "rdLMTp%'0igaV$MhSM'G&l?-"
    "TQ8C%DdiD,J]B.*L2fcr=a@+0s#YCjYtlS7m.%A#8.-xO8BAZMWpS+MUX-##tMe>5Uqv*1$/"
    "qbM*g)n5:u@na.@1l'&?Lm0D*[p*LYU$-ZGgn]V`&JNsiTP#''BKW0jY4S)QIb@NWBW@"
    "CE.@-':*/"
    "12rC$#X^sH$kF.%#E&eB*[B:h1FGh+2.s$*3mtg%O&cSgL.cw7&ZSG<$n@m#u@^[h(X1ub'8djjL_0]u-hk%rM]F`t-C2#LMO`sfL(>SS%NV%"
    "JLZu^##sRa*+3+GS7=;4GD"
    "poMfL39<U/ak4D#dbL+*t.4I)G&Sn$kAL7/,39Z-+dfF4Y7t[$^@/T%>L75/K4_Y,E%AA4``MB#3Of@#BY(?#H7Gm'SiWI)iRm5/"
    "s+?)4G4(gLB'IAOgk-AOQ`m>#I3XIH[$l2vQ8)S&"
    "6D,##?Ags%YQbi(S+l%l^%7T.xf,X$e2Lh$#jgv):@XJ(?Eg:RI3$ipojS@#^o2lSHAsg96^kA#AIVX-<uqveGWx(<Sc0PO&b2+r%ZnA#2jh%/"
    "-A5>#2IfBJ$$#+%R4oeOCRL).WK#`M"
    "J^AeI](%G>VxG:.>_,w+iQ*r$Sa7kpRB7o[AYep(4sEt&:H$ip>IH*R@DaSM'wR4]fb5F7&`#AOsZ94PIA_l8c0J(6)Rio.aa#V/"
    "&@Gm'U(KF*+VY8.6p;+34^.*3f>?Yc-gVu59#ThC"
    "(0CD30GShL/4O%$E-[Q'$6'C/dvMhL?<xu-0;H&MvVh[-pk8`-l-7##SWAe$E6###,vv(#14n8%@f)T/hPF/"
    "MR[vY#WR4UgY)E.3<vcG*TuZg)cvLG)7uAgLUu^H4;xI`,fR0+*k[x&>"
    "][<=@I5B,=+Bd)SE/"
    "Y+Z`^w'.a5St9bP1f3bKuGO?7d],I[Tu.T9Vn:.CY>#9og8KPW_>-J]`$'JDafqISxi,rMBs7SFHK36fFgc*UK2;KvOl(64@pEvBH/"
    "(6x:$#Yo]E[4#J/LVl#v#"
    "5FNP&2P###l$CN(.awiL+ru8.$&Mu>,WZp.rd8rH$09rHUs=rH^KH_##rBf#v+1/u'EfoR#,;7nnR(##Ql1R<&e8e$D=%u/Nww%#RI5+#?CtR/"
    ";X^:/d'F=$+($Z$@LrP8woD[$$_Aj0"
    "Sg'u$TekD#EM#<-UX`=-_d$^4T9OA#oSfF44qVa4Zp?A4/"
    "H+G4XC)?##7]h,.fgF4JH^@#O6hF*mf(G`:bR=-^.;i0-Q?D6HOk[7.%m`>Q_1p0*'bf:BmarZpmN.C_Vl?%6U`/:Tn29:"
    "J'O9%^mr--0[5n0BYGG*fSMt:TS@Uf00:]=Gs.p09M^?6t'[q1-_5@']wne$#cS_#6GHm4D?AJcG=]9%95L[0918t$v/"
    "O4Sk66K1p9>)#7iLvu(GL7%+Sl##UNtA#BXR@#(ddC#`*a<#"
    "3.i?#aK7f3T&Du$RrK<hA5OQ'8Fn8%7+TC4-fK;6*s_`3]HJ5/Yq[P/:.2-*:mp0,Z&tj1*un$$x3p:/"
    "mfBk1sO,G4]#[)*O5K+4>3*w$J(Fj1)dWjL_VOjL=HPA#tE4v-J]OjLi.F+3"
    "B4)=-7`k?/hk$],+0ihLqs+G4`e%s$VY1O+TNEH*nJ1b4>2^F*QUe8%2WWjL7L]s$]q.[#hcq@-@``6Mnf+G46='^,ppYh#$p=Z,B_4V/"
    "tPYx6pm,D-[39oL+B'g-xPH$KP.]w'e<uD#"
    "NSpHQ8c^F*@*qD3[`tZgB$j*.jb4V/8IA8%F./bIG$VhL.K7q.L=5Q';?([KJ9..MFQSF4s*#,MU#TfLjh/"
    "r-G1?']t9eKuL.H9i8ZIl1Y8H$KRv=)4sji;%Y-6l.[SwD*e['o-tBA$T"
    "wG6C#ji8T/"
    "A-+mL:;n]4DBbQjCM_D4)^Nq2_.I,*fqvP'-L?h>K>sae&PGI?x,p<C>-pY$&*,q%D'XX$G:g6&EG>Yuwv$W$:[s5&Ee*=$)>P>#`tVf44f:v#"
    "6:wo%F<(@.*ji`-.1,m'"
    "M8[d)PL`?#-=_+*JaYR&E@5#%aF>(+$nTQ&]3xW$t.J1(U]wo%j4Z>#Dw.A,@KCK(>4r;$GR.@#b4xL(OH-)*]/"
    "LB#MBPN'uUp+*aQXM0mHFp%?@3p%@j[w%dFpC4?h4/(Hat6/BFq+*"
    ":l-s$H9p2's<[I49'N-*c6IA,SYZF<4f$s$eG_>$4%%W$$MNj1iiiG)=g[w#=*9N05iI?$:V(v#=?2XILBwR&'JQ-HANHd)A1mY#[5.l'/"
    "'%0<&-pP&7f1v#>LjP&xe%l#HR7W$9i:Z#"
    "qB6k(K2EH&<uq;$<Oe<$iFA]$K-Kq%6VuY#4rL;$E-'U%De.[#1Lx[#)E9r.<tw5/"
    "l$AhC'?Z7&;<._>ne3113mC(,8Au2)vm8e*UmtA#91D?#MCVS(Nu/(4L1Os%P''m&Oitr-`X#K)"
    "DRJ1C'G=n0V,?7&><XQ16JY>#[MH;%7pK7/k0Bn&X6(f<+<L.EvGw^$&/5mBv(3U%PKB6&]CWx-l2hq%pZJ7/mx3.)=UCv/"
    "pt`-4PQB?-wmgM'[=pB+ctYJC>C%s$sH+U%vrjP&2n8]="
    "B$FX$<1M?#AU%w#7iLv#mA5iTvRT'+v];8&'j2[$?HRI)L34#0&9`9J)r<&-+)or6#um,*NR4W%F7H>#87*v#Kjik't3G^$2m-r%gf6w':.[8%"
    "KOI>#bjY01RIjP&^+0/)-UNM0i^(O'"
    "UBTM'b23E*@]cY#Jk/q%QtEt$#<GT7OjT>$F7e8%<$5j'HCsl&_^Kq%7xhV$5oUv#vCX<6=fts6U:R8%Ekjq8(/,P5gaQa*^O[),TlG>#/"
    "aXw&;*NFN<%]q/Lpri2aNiZ%BuhV$xWZ4("
    "S*YW%_M'S8l,2,)6ebeD#.bq.&(aM0R:mY#Akoi'_219%7FN5&3PcY#p?e>#2O^K(4c_;$FBkx#mjVs#B$#j'D$=t$?IIs$;C7w#A%JUdvkK+*"
    "lD;4'A?Xh:hvLg(qCfL()AY>#$qD)+"
    "oZQ8'E0Tm&X74x>>ort.aUR<$.ZKv-Q-cX$mvxN0d%=**:jaT']DW&+`unP'7k:e3[bF*axwOs%)4V@,S4Ye)nlZN)te,fc9Y?h)1D;P03e,BI`"
    "u[rfq#At/oDGs.KuZS%e#_Y$7:Nf4"
    "pRXp9H@E=.nN7b1G$fg:t%TG3w>vj'i[Ndt&g>&#:'PJ((].C#VA3I)MhVp@t(MB#CU)P(qq4D#NNg@#qi./"
    "L$#nILt'PQu^I1_$`>#Au3Yw),O=iV.:lI`a(?#]#@LTt@UTglJ's/uu"
    "xxx=#=9kZupi%@#jSS>.;j=98:oQS%9OSM'=4n0#NwBwK#v^##4aR3Os,@D*DV%8@d3cT8kN%##__G)45HdA'P[TfL,ibe)[o6F%F*nl+;@$Z$"
    "U]L/)VjrB#%Qik#J[29/lrn+MIO/GV"
    "-<E:lG;4V-ng`X-;D2`&m@9)lxkx?LasI73TBdA#0=L^#w<)0u#og>$j48Y-f7w@'F_P&#aM^[30oqr$@'+&#N`aI)IE^:%U#LB#ZJA9/"
    "7cJD*gHILMtt@T%SX11/Mdf@SL$x:6(jjA^"
    ":N992TEDN#9`rq:Mb]@oF(r,EGqRgD;`NrLiYSqJP7YM_;u5F.GVVj:trfV/p11)/:H?D*B4$W%W:h&4q$),#BB+.#X)1/"
    "#qxQ0#1`W1#KX#3#a?)4#pji4#(E]5#B8u6#N]U7#E.>>#"
    "G%`v#0j#f$_f)T/&hs-$b]d8/r'l%$4-#f$rit1)c=2=-7i:p.+g^I*VMu-$,.ikLHji,)lYWI)&l0B#jW'B#5:cA#R+aH*C#*w$dhlG*EW@Y/"
    "ds>V/2Ik%$jeNc49v?A4(CZV-e^=x#"
    "6Wqi'><q=%7qq-MP'AI*>U1F*[KA8%C@+w$2&oD*iC$p.h_e8%,^N.)c6NZ6urLT/g.W%$n^A@#C3q+M)+Oc4'D]C4Yu/"
    "J)w$'J)w#2d*gEj=G#wjZu,p,G*uqv[)xgA^)3KVwcrEUQ$"
    "3SOi(LiwB4*xou(OMY,M1lZ;%^2&%u%C3hu])CuYtEAO0)QfY#&^_?`@OgxX`)vn#E,vG(WRxP#AZE[,dg*ci@0h;(e-MG#;xV=G)'2cu((7h("
    "KXo`uGOYXW.*[S%2$[N#f+1S$eP]v#"
    "<(oRR&@BU#,_tAue;Nrmt#to.E%_>#u*Zn$wEGF#fgSXW43s@$;/"
    "Z$0NHluYN=4Z#2CU>$&0pp%MvQ#5O=3E**2OI)#a-V%.'`C$i<kp%W2Jv,`l.N1,WF;$XHs?#dKNa*1tn0#M)4d*"
    "$a5iTi+N4'FaHO$dP<T#3o(@(O6=p%u8T1TMe48@FRP>#0uOD()v9W$w?RI#:;Dj7%6BA#[CFM$?YEX%A7=W$5%FE#QRr$ak.6&47k4/"
    "(Ea>2'?mW`U*;#ZknC1+;1_`#lJEtXcn@ID3"
    "N.nUmb)M`NARQc$VAM0(JX-K#VD?_u`+Bwk:P]@O$*i]+2.oE#Fdc>#Y^tAub3UG;Y-&=%S6[0#9@%%#%IuM(/"
    "eFc%;lEb35IL,3?SUV$%P$s$jMhGtcu%?AndPI8ZdB##Z/rbM-4pfL"
    "]l&-#f]e^$@BkLWLK)e&G3n7[KSae)QXkD#X8Pd3t<7f3E6o<-sHu8&0SMg)<Lp+MaPsI3QNCD3Xq)((3G.;(=wNh#Yo.I%?PY551d@+0j.8$$"
    "pL=D#':MLMGG-##)Fi0(qOX]X>xhQt"
    "adO&5@pFnEw@b,M@V3B-/"
    "O$<-r+TV-n+d3#Xke&#';P>#,Gc>#0Su>#4`1?#8lC?#<xU?#@.i?#D:%@#HF7@#LRI@#P_[@#Tkn@#fuaT#Z'4A#_3FA#c?XA#gKkA#$=QSDs*"
    "$1FowRiF"
    "RQ]JCkJ$d$c?DtB#4Ke$]$WiFxS^oDl>_G$&G`*HnF+7Du];]&pXf,;q1D.G&90NB/NbRD$K_(%6mpKF[s4gC(dXVCpr#oDfZ/"
    "jB.jF;Crt5hF'g1eGa4lF$+oRFH&h7FHsV,-GhnL*H"
    "8lV.Gvq>lECFaSAlIc#Ha^jjE=aE=(6S4VCD,m<-$?rX/ug`=B$1SMF/73j(TW#LP(aF=%,-]:C/"
    ",RD%u()eGf?=b$iLCZH3Daj3gS1I$4[RlL_YoF4lhWC5B3SMFI7'r8>3SMFI7'r8"
    "kPUV$UqH._n%###";

  static const ImWchar ranges[] = {
    0xf00a,
    0xf2db,
    0,
  };

  ImFontConfig config {};
  config.OversampleH = 1;
  config.OversampleV = 1;
  config.PixelSnapH = true;
  config.SizePixels = 16;
  config.GlyphOffset.y = -4.0f;
  config.MergeMode = true;

  // copy font name manually to avoid warnings
  const char* name = "icons";

  std::size_t charsToCopy = std::strlen(name) + 1;
  assert(charsToCopy <= sizeof(config.Name) && "Output buffer is not large enough.");
  std::strncpy(config.Name, name, charsToCopy);

  return ImGui::GetIO().Fonts->AddFontFromMemoryCompressedBase85TTF(
    iconsCompressedDataBase85, config.SizePixels, &config, ranges);
}
