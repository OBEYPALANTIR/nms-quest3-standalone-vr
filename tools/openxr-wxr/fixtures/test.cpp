// Mac-native unit test of WinXrApiUDP source policies (fake Winsock; no sockets bound)
#include "WinXrApiUDP.h"
#include <cstdio>
#include <cmath>
#include <cassert>
unsigned long long g_fakeTick = 100000;
static std::string bin(float ipd, float rtrig, int frame, float headX, bool a, int aimFlags = 3) {
  std::string b("WXR\x07", 4);
  auto putf=[&](float f){ b.append((const char*)&f,4); };
  auto puti=[&](int32_t i){ b.append((const char*)&i,4); };
  for (int i=0;i<12;i++) putf(i==8?ipd:(i==6?rtrig:(i==9||i==10?95.f:(i==11?72.f:0.f))));
  puti(frame); puti(0); puti(a?(1<<10):0);
  puti(3);
  // head pair (0,106), head VIEW pair (0,108), and controller pair (202,106)
  puti(0); puti(106); putf(headX); putf(1.6f); putf(0); putf(0); putf(0); putf(0); putf(1); puti(3); for(int i=0;i<6;i++) putf(0);
  puti(0); puti(108); putf(0); putf(0); putf(0); putf(0); putf(0); putf(0); putf(1); puti(3); for(int i=0;i<6;i++) putf(0);
  puti(202); puti(106); putf(9); putf(9); putf(9); putf(0); putf(0); putf(0); putf(1); puti(aimFlags); for(int i=0;i<6;i++) putf(0);
  return b;
}
static std::string txt(int frame, float rtrig = 1.0f, bool a = true, float hmd_qy = 0.0f, float hmd_qw = 1.0f) {
  char p[2048];
  snprintf(p, sizeof p,
    "CLIENT0 0 0 0 0 0.000 0.000 %.3f 0.000 0.0640 95.0 95.0 72.0 %d 0 %s 3 "
    "0 106 0 0 0 0 %.4f 0 %.4f 3 0 0 0 0 0 0 "
    "0 108 0 0 0 0 %.4f 0 %.4f 3 0 0 0 0 0 0 "
    "202 106 0.15 -0.10 -0.40 0 0 0 1 3 0 0 0 0 0 0",
    rtrig, frame, a ? "FFFFFFFFFFTFFFFFFFT" : "FFFFFFFFFFFFFFFFFFF",
    hmd_qy, hmd_qw, hmd_qy, hmd_qw);
  return p;
}
static const WxrHostPose* find(const WxrHostState& s,int a,int b){ for(auto&p:s.poses) if(p.space==a&&p.baseSpace==b) return &p; return nullptr; }
int main(){
  int fails=0;
  #define CHECK(c,msg) do{ if(!(c)){printf("FAIL %s\n",msg);fails++;} else printf("ok   %s\n",msg);}while(0)
  {
    WinXrApiUDP defPol;
    defPol.RegisterActionSpace(202);
    WxrHostState s;
    std::string b=bin(0.0702f,0.0f,5,0.25f,false,3); defPol.StorePacket(b.data(),(int)b.size());
    g_fakeTick+=2; std::string tIdle=txt(10, 0.0f, false, 0.0f, 1.0f); defPol.StorePacket(tIdle.data(),(int)tIdle.size());
    defPol.GetState(s);
    const WxrHostPose* head=find(s,0,106); const WxrHostPose* aim=find(s,202,106);
    CHECK(s.floats[8]>0.07f, "default policy=1(merge): idle text packet preserves host IPD 0.0702");
    CHECK(head&&head->pose[0]==0.25f, "default policy=1(merge): idle text packet preserves host 6DoF head_x=0.25");
    CHECK(aim&&aim->pose[0]==9.0f, "default policy=1(merge): idle text packet preserves tracked host controller aim_x=9.0");
  }
  for (int policy=0; policy<3; policy++) {
    WinXrApiUDP u; u.RegisterActionSpace(202); u.SetSourcePolicy(policy); WxrHostState s;
    std::string b=bin(0.0702f,0.0f,5,0.25f,false,3); u.StorePacket(b.data(),(int)b.size());
    g_fakeTick+=10; std::string t=txt(11, 1.0f, true, 0.0f, 1.0f); u.StorePacket(t.data(),(int)t.size());
    u.GetState(s);
    const WxrHostPose* head=find(s,0,106); const WxrHostPose* aim=find(s,202,106);
    printf("policy %d: ipd=%.4f rtrig=%.2f A=%d head_x=%.2f aim_x=%.2f frame=%d\n",policy,s.floats[8],s.floats[6],(int)s.buttons[10],head?head->pose[0]:-99,aim?aim->pose[0]:-99,s.frameId);
    if(policy==0){ CHECK(s.floats[8]>0.063f&&s.floats[8]<0.065f,"legacy: latest(text) wins ipd"); CHECK(head&&head->pose[0]==0.0f,"legacy: text head"); }
    if(policy==1){
      CHECK(s.floats[8]>0.07f,"merge: host ipd");
      CHECK(head&&head->pose[0]==0.25f,"merge: host head");
      CHECK(s.floats[6]==1.0f&&s.buttons[10],"merge: injector trigger+A");
      CHECK(aim&&aim->pose[0]>0.14f&&aim->pose[0]<0.16f,"merge: active injector aim pose");
      CHECK(s.frameId==5,"merge: host frame id");
      // Test Gate (d) synthetic HMD rotation via vr_cmd.txt
      g_fakeTick+=2; std::string tRot=txt(12, 0.0f, false, 0.1305f, 0.9914f); u.StorePacket(tRot.data(),(int)tRot.size());
      u.GetState(s);
      const WxrHostPose* headStage=find(s,0,106); const WxrHostPose* headView=find(s,0,108);
      CHECK(headStage&&std::fabs(headStage->pose[4]-0.1305f)<0.002f,"merge: synthetic HMD yaw rotates STAGE (0,106)");
      CHECK(headView&&std::fabs(headView->pose[4])<0.0001f,"merge: synthetic HMD yaw keeps VIEW (0,108) identity");
      g_fakeTick+=600; std::string b2=bin(0.0702f,0.0f,6,0.30f,false,3); u.StorePacket(b2.data(),(int)b2.size()); u.GetState(s);
      CHECK(s.floats[6]==0.0f&&!s.buttons[10],"merge: stale injector (>500ms) falls back to host controls");
      aim=find(s,202,106); CHECK(aim&&aim->pose[0]==9.0f,"merge: stale injector -> host aim pose");
    }
    if(policy==2){ CHECK(s.floats[8]>0.07f&&s.floats[6]==0.0f,"host_only: text ignored"); auto st=u.GetSourceStats(); CHECK(st.textDropped==1,"host_only: drop counted");
      g_fakeTick+=1500; std::string t2=txt(12); u.StorePacket(t2.data(),(int)t2.size()); u.GetState(s); CHECK(s.floats[6]==1.0f,"host_only: host silent >1s -> text accepted"); }
  }
  // merge with no host at all: injector alone must still drive state (no hang)
  { WinXrApiUDP u; WxrHostState s; std::string t=txt(13); u.StorePacket(t.data(),(int)t.size()); u.GetState(s); CHECK(s.floats[6]==1.0f,"merge: no host -> injector state"); }
#ifdef TEXT_PRIORITY_INPUT
  // text_priority_input: host trigger stuck at 0.96 with A held, injector releases both
  for (int on=0; on<2; on++) {
    WinXrApiUDP u; u.SetTextPriorityInput(on); WxrHostState s;
    std::string b=bin(0.0702f,0.96f,7,0.25f,true); u.StorePacket(b.data(),(int)b.size());
    g_fakeTick+=2; std::string t=txt(14, 0.0f, false); u.StorePacket(t.data(),(int)t.size()); u.GetState(s);
    if(!on) CHECK(s.floats[6]==0.96f&&s.buttons[10],"text_priority_input off: max/OR keeps host 0.96 and A");
    else CHECK(s.floats[6]==0.0f&&!s.buttons[10],"text_priority_input on + fresh: injected 0.0 and A up win");
    if(on){ g_fakeTick+=600; std::string b2=bin(0.0702f,0.96f,8,0.25f,true); u.StorePacket(b2.data(),(int)b2.size()); u.GetState(s);
      CHECK(s.floats[6]==0.96f&&s.buttons[10],"text_priority_input on + stale (>500ms): host 0.96 and A"); }
  }
#else
  printf("skip text_priority_input checks: 0006 not applied in this output\n");
#endif
  printf(fails?"RESULT FAIL %d\n":"RESULT PASS\n",fails); return fails;
}
