#include "mlvpn.h"

/* Fairly big no ? */
#define MAX_TUNNELS 128

struct mlvpn_wrr {
    int len;
    mlvpn_tunnel_t *tunnel[MAX_TUNNELS];
    double ratio[MAX_TUNNELS];
    double total_bandwidth;
    uint64_t bytes[MAX_TUNNELS];
    uint64_t total_bytes;
};

static struct mlvpn_wrr wrr = {
    0,
    {NULL},
    {0},
    0.0,
    0
};

/* initialize wrr system */
int mlvpn_rtun_wrr_reset(struct rtunhead *head, int use_fallbacks)
{
    int i;
    mlvpn_tunnel_t *t;
    wrr.len = 0;
    wrr.total_bandwidth = 0.0;
    wrr.total_bytes = 0;
    LIST_FOREACH(t, head, entries)
    {
        if (t->fallback_only != use_fallbacks) {
            continue;
        }
        /* Don't select "LOSSY" tunnels, except if we are in fallback mode */
        if ((t->fallback_only && t->status >= MLVPN_AUTHOK) ||
            (t->status == MLVPN_AUTHOK))
        {
            if (wrr.len >= MAX_TUNNELS)
                fatalx("You have too many tunnels declared");
            wrr.tunnel[wrr.len] = t;
            wrr.ratio[wrr.len] = t->bandwidth;
            wrr.bytes[wrr.len] = 0;
            wrr.total_bandwidth += t->bandwidth;
            wrr.len++;
        }
    }
    
    for(i = 0; i < wrr.len; i++)
        wrr.ratio[i] /= wrr.total_bandwidth;

    return 0;
}

mlvpn_tunnel_t *
mlvpn_rtun_wrr_choose(uint32_t pktlen, uint32_t mtu)
{
    int i;
    int idx = 0;
    double max_deficit = 0.0;

    if (wrr.len == 0)
        return NULL;
    
    for(i = 0; i < wrr.len; i++)
    {
        double expected = wrr.ratio[i] * (wrr.total_bytes + pktlen);
        double deficit = expected - wrr.bytes[i];
        if(deficit > max_deficit)
        {
            max_deficit = deficit;
            idx = i;
        }
    }
    
    wrr.total_bytes += pktlen;
    wrr.bytes[idx] += pktlen;
    return wrr.tunnel[idx];
}
