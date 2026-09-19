// Published only after the existing local native acquisition finishes. TLS
// prevents calling the native query's shared sorting work on another thread.
struct Halo2IndependentQueryContext
{
    uint32_t unit=UINT32_MAX,generation=0;
    uint64_t sampleMs=0;
};
thread_local Halo2IndependentQueryContext g_halo2IndependentQueryContext;
void RecordHalo2IndependentQuery();
