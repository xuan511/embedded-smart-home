// SPDX-License-Identifier: MIT
#include "home_remote.h"
#include <assert.h>
#include <stdio.h>

static hl_message_t status(home_remote_t *r, int32_t seq)
{
    return (hl_message_t){.type=HL_STATUS,
        .v={r->session,seq,0,2,1,1,1,3000,5500,500,7,0}};
}
int main(void)
{
    home_remote_t r;
    hl_message_t tx;
    home_state_t view;
    bool online, stale;
    home_remote_init(&r, NULL);
    home_remote_attach(&r, 100, 10);
    assert(home_remote_poll(&r,10,&tx) && tx.type==HL_HELLO);
    assert(!home_remote_request(&r,HL_SET,0,1,11,&tx));
    hl_message_t ready={.type=HL_READY,.v={100,4}};
    home_remote_receive(&r,&ready,20);
    assert(home_remote_poll(&r,20,&tx) && tx.type==HL_CONFIG);
    int32_t first_config=tx.v[1];
    home_config_t cfg=home_default_config(); cfg.fan_on_c=29;
    home_remote_config(&r,&cfg); // A late old CONFIG ACK cannot mark new settings synced.
    hl_message_t ack=status(&r,first_config);
    home_remote_receive(&r,&ack,30);
    assert(!r.synced);
    assert(home_remote_poll(&r,30,&tx) && tx.type==HL_CONFIG && tx.v[2]==2900);
    ack=status(&r,tx.v[1]);
    home_remote_receive(&r,&ack,40);
    assert(r.synced);
    home_remote_view(&r,40,&view,&online,&stale);
    assert(online && !stale && view.light_on && view.fan_on);
    assert(home_remote_request(&r,HL_SET,0,0,45,&tx));
    home_remote_view(&r,45,&view,&online,&stale);
    assert(view.light_on); // Request is not optimistically applied to reported output.
    ack=status(&r,tx.v[1]); ack.v[5]=0;
    home_remote_receive(&r,&ack,50);
    assert(!r.reported.light_on && !r.pending);
    // Repeated fresh link messages with an old sensor sample must remain stale.
    ack=status(&r,0); ack.v[11]=5000;
    home_remote_receive(&r,&ack,100);
    home_remote_view(&r,100,&view,&online,&stale);
    assert(online && stale && view.temperature_fault && view.light_fault);
    assert(view.fan_on); // Korvo never recalculates STM32 outputs, even during stale data.
    assert(home_remote_request(&r,HL_SET,1,0,110,&tx));
    int32_t old_session=r.session, old_seq=tx.v[1];
    assert(home_remote_poll(&r,1610,&tx) && tx.type==HL_HELLO);
    assert(r.session!=old_session && !r.synced && !r.pending);
    ack=status(&r,old_seq); ack.v[0]=old_session;
    home_remote_receive(&r,&ack,1620);
    assert(!r.have_status); // Old-session status cannot revive controls or data.
    home_remote_detach(&r);
    home_remote_view(&r,1700,&view,&online,&stale);
    assert(!online && stale && view.temperature_fault);
    // Millisecond rollover does not break timeout calculations.
    home_remote_attach(&r,200,UINT32_MAX-100);
    assert(home_remote_poll(&r,UINT32_MAX-100,&tx));
    ready.v[0]=200;
    home_remote_receive(&r,&ready,UINT32_MAX-90);
    assert(home_remote_poll(&r,UINT32_MAX-90,&tx) && tx.type==HL_CONFIG);
    ack=status(&r,tx.v[1]);
    home_remote_receive(&r,&ack,20);
    home_remote_view(&r,30,&view,&online,&stale);
    assert(online && !stale);
    // Exhaustion must establish a new session before sequence 1 is reused.
    r.sequence = INT32_MAX;
    assert(!home_remote_request(&r,HL_SET,0,1,31,&tx));
    assert(home_remote_poll(&r,31,&tx) && tx.type == HL_HELLO && r.session != 200);
    cfg = home_default_config(); cfg.fan_on_c = 28.004f;
    home_remote_config(&r,&cfg);
    assert(r.desired.fan_on_c == 28.0f);
    // A collapsed hysteresis must not replace the desired configuration.
    cfg.fan_on_c = 28.002f; cfg.fan_off_c = 28.001f;
    home_remote_config(&r,&cfg);
    assert(r.desired.fan_off_c == 26.0f);
    puts("Korvo remote state tests: PASS");
    return 0;
}
