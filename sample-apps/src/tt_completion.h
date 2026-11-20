#ifndef TIMPANI_TT_COMPLETION_H
#define TIMPANI_TT_COMPLETION_H

#define TIMPANI_TT_COMPLETE_OP (10 | 128)
#define TIMPANI_TT_COMPLETE_BITSET 0x5444434dU

struct timpani_tt_completion {
    unsigned long long deadline_ns;
};

#endif