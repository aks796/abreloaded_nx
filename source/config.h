/* config.h -- Plants vs Zombies Fusion 3.6.1 Switch wrapper configuration
 * (forked from the Zookeeper DX / CR3 wrapper config.)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

/* ============================ MEMORY LAYOUT ==============================
 * These are engine-fitting parameters, not game content -- identical to the
 * Zookeeper DX port because Fruit Ninja Classic + is the SAME Unity minor version
 * (2022.3.62) and we apply the SAME 256MB->64MB region-granularity patch
 * (see nx_patch_abr.h). Do not change unless you know the allocator math.
 * ======================================================================== */

// The engine + libc++ + il2cpp heap need a generous newlib heap; the rest of
// system memory is handed to the .so loader (see __libnx_initheap).
#define MEMORY_MB 768

// Anonymous-mmap arena. Unity reserves big region-aligned pools by over-mmapping
// then munmapping the unaligned head/tail. We back anonymous mmaps from a
// dedicated, region-aligned arena with a per-page used-bitmap so sub-range
// munmap frees exactly the trimmed pages. Region granularity is 64MB to match
// the libunity patch.
#define MMAP_ARENA_ALIGN    ((size_t)64 * 1024 * 1024)    // 64MB region granularity (libunity patched 256MB->64MB, nx_patch_abr.h). NOTE: 16MB was tried and CORRUPTED Unity's Dynamic Heap allocator at init (overlapping regions from the over-map/trim pattern -> null-prev free-list crash). 64MB is the known-good floor.
/* Round 152: 192 -> 896 MB. The r151 log finally produced the failure this
 * comment has predicted for dozens of rounds, with the exact recorded
 * signature:
 *
 *   [oc] ARMED: window 704 MB          <- only 704 this boot; 1152-1600 in others
 *   [mem] arena 81% reserved (157 of 192 MB)
 *   [mmap] OC window full for 319 MB -> heap-backed arena
 *   [mmap] arena full -> newlib fallback (#1) -- memory pressure
 *   [mmap] 127 MB (prot=0x0 anon=1) -> 0x0     <- mmap returned NULL
 *   [mmap] arena full -> newlib fallback (#2) -- memory pressure
 *   -> crash in libunity
 *
 * "couldn't fit a 127MB OC-window overflow -> mmap NULL -> Unity crash" is
 * word for word what 512 MB did. We were running 192.
 *
 * It also explains the intermittency: the OC window is clamped to the largest
 * stack-region hole, which is different every boot (704 MB here, 1152-1600 in
 * runs that were fine). A big window absorbs the spill; a small one pushes it
 * into this arena, which at 192 MB could not take it. "A couple of perfect runs
 * then a crash" is that lottery.
 *
 * 896 is the value this comment has recommended all along = 1.75x the 512 fail
 * point. Anything at or below 512 is KNOWN to fail -- do not "compromise" at
 * 400. Paid for by right-sizing OC_POOL_BYTES below. */
/* ROUND ABR-2: 896 -> 640 MB, to fund GPUA_BYTES back up to 1152.
 * The long comment above is Fruit Ninja's history and warns that <=512 is
 * KNOWN to fail there. 640 stays above that line. Justification for moving it
 * at all: across a full Angry Birds run that reached the in-game HUD and 857
 * rendered frames, this arena logged ZERO pressure -- not one "arena full" or
 * "newlib fallback" line -- while the GPU arena hit its ceiling and started
 * failing 64 MB allocations. Memory is being held where it is not needed and
 * starved where it is. Shrinking this arena feeds the newlib heap directly
 * (main.c: fake_heap_size = size - so_zone - arena_sz - big_align), which is
 * where both GPUA_BYTES and OC_POOL_BYTES are drawn from.
 * If "[mmap] arena full -> newlib fallback" ever appears, put this back. */
/* ABR round 5: BACK TO 640. Round 4 took this to 512 on the argument that it
 * had "logged zero pressure". That argument was wrong, and the FN history above
 * was right: the failure mode of an undersized mmap arena is NOT an "arena
 * full" log line, it is an mmap returning NULL that Unity mishandles, and it is
 * INTERMITTENT because the OC window is clamped to the largest stack-region
 * hole, which differs every boot. Exactly as the comment predicts, five boots
 * at 512 gave three hard hangs with the GPU arena never even initialised and
 * two runs that managed 42 frames. 640 is the value that last reached actual
 * gameplay. Absence of a log line is not evidence of absence. */
#define MMAP_ARENA_RESERVE  ((size_t)640 * 1024 * 1024)

// Stack-region overcommit (OC) arena (see libc_shim.c): PROT_NONE reservations
// held in a stack-region window, committed pages backed from a small heap pool.
#define OC_WINDOW_BYTES     ((size_t)2048 * 1024 * 1024)  // 32x64MB cheap PROT_NONE reservation. Was 1536; the window-finder clamps to the largest stack-region hole (min(this, hole)), so raising the cap lets a run use its full hole and spill fewer reservations into the (real-memory) arena.
// Commit-pool: real memory backing touched pages of the OC window. Unity is told it
// has 512 MB (libc_shim.c __sysconf PHYS_PAGES + dalvik.vm.heapsize), and it reserves
// its heaps as big PROT_NONE regions that route here, so this pool must be able to
// back the full 512 MB Unity believes it has -- at 256 MB the scene load exhausted it
// (~270 MB working set) and the next uncommitted page faulted -> hard OOM crash.
// malloc. UPDATE 2: trimming the pool 1280->1024 + arena 1024->512 fixed the malloc
// OOM (malloc 1409 -> game pushed past the null-buffer crash) but 512 starved the
// arena (see above). That round settled on pool 896 + arena 896 + malloc 1153.
//
// UPDATE 3 (round 107): those last two no longer describe the tree. The ACTUAL
// values are pool 512 and MMAP_ARENA_RESERVE 896 as of round 152. (Between
// r107 and r152 they were pool 1024 / arena 192, and neither comment said so --
// the recorded "balance" was fiction for 45 rounds. It is accurate again now.) Against the observed 2752 MB newlib heap that gives
// pool 1024 + arena 192 + so_region 160 = 1376 MB, leaving ~1376 MB for malloc.
// Known failure points, for reference when one of these OOMs again:
//   pool live 551 | arena fail 512 | malloc fail 641
// Note the arena is now BELOW its recorded failure point. It has been booting, so
// the enlarged OC window is evidently absorbing what used to spill there, but it
// is the next thing to look at if an allocation failure shows up in the arena.
// RAISED 896 -> 1024 (round 107). Fruit Ninja's live footprint is not PvZ's --
// 244 MB observed at ModeSelect here, but the out-of-memory crash happened later,
// in play, and this is the pool that has to absorb it. +128 MB comes out of the
// plain-malloc share, which currently sits ~1376 MB against a recorded failure
// point of 641 MB, so there is room to give.
//
// Safe to raise now: main.c retries in 128 MB steps down to OC_POOL_MIN_BYTES if
// the allocation does not fit, and logs what it settled on. Previously a pool
// that was too big meant OC DISABLED and a dead boot, which is why this number
// had not been touched.
/* Round 152: 1024 -> 512 MB, to pay for the arena above without asking newlib
 * for more in total. 512 is this comment's own documented floor -- "must be
 * able to back the full 512 MB Unity believes it has" -- and the r151 session
 * peaked at `[oc] committed 235 MB (pool 235/1024)`, so it never used even half
 * of 512. The pool was over-reserved by ~4x while the arena starved.
 *
 * Budget against the observed 2752 MB newlib heap:
 *     before  pool 1024 + arena 192 = 1216   -> malloc ~1536, arena OOM'd
 *     after   pool  512 + arena 896 = 1408   -> malloc ~1344, still above the
 *                                               1153 the history settled on
 * If a scene ever pushes the pool past 512 the symptom is a hard OOM on an
 * uncommitted page (see above), and the fix is to take it back off the arena. */
/* ROUND ABR-1: 512 -> 768 MB. This is the failure the comment above predicts,
 * observed on Angry Birds Reloaded's first scene load:
 *
 *   [oc] committed 512 MB (pool 512/512 MB, recycled 0 MB free)
 *   [oc] commit-pool EXHAUSTED: needed 640 pages (live 512 MB of 512 MB pool)
 *   [xd] pc=libunity+0x71d0d0 far=0x2f57ffffe0 esr=92000046  (WRITE, unmapped)
 *
 * The pool climbed monotonically to 512 and died still climbing, so 512 is a
 * floor, not a peak -- 768 gives 50% headroom. Nothing else was under pressure
 * in that run: zero arena-full lines and zero malloc failures, so this is NOT
 * paid for out of MMAP_ARENA_RESERVE (which the comment above says must stay
 * above 512). It is paid for out of the GPU arena, which is sized 1152 MB from
 * FRUIT NINJA's measurements and has never been measured for this game -- see
 * GPUA_BYTES in nx_alloc.c. The OC pool is allocated in main.c BEFORE
 * gpua_init() runs lazily at first GPU use, and gpua has its own shrink ladder,
 * so the GPU arena absorbs whatever is left rather than failing outright. */
/* ABR round 7: 704 -> 640 MB. The r6 crash was plain-heap starvation:
 * GPUA=1280 + OC=704 left only ~320 MB of the measured 2304 MB newlib heap,
 * followed by `memalign FAILED size=4 KB` and an uncaught std::bad_alloc.
 * The same run consumed 552 MB of this pool; the longest prior run reached
 * 611/768 MB allocated (545 MB live + 65 MB recyclable), so 640 preserves the
 * observed high-water while returning 64 MB to general malloc. Keep the mmap
 * arena at its independently known-good 640 MB. */
#define OC_POOL_BYTES       ((size_t)640 * 1024 * 1024)   // commit-pool (touched pages only)
/* Ladder floor for the 128 MB step-down in main.c. NOTE: this was 640 while
 * OC_POOL_BYTES was 512 -- i.e. the floor sat ABOVE the starting size, so
 * `if (pool || pool_sz <= OC_POOL_MIN_BYTES) break;` always broke on the first
 * iteration and the ladder never actually stepped. It was dead code. With the
 * start at 640 the floor must be genuinely below it for the retry to exist;
 * 384 lets it walk 640 -> 512 -> 384 before giving up. */
#define OC_POOL_MIN_BYTES   ((size_t)384 * 1024 * 1024)

// Overcommit (alias-region) mode: reserve a big *virtual* window (PROT_NONE
// costs only address space) and commit physical pages on demand -- true
// overcommit, matching Android.
#define MMAP_VIRT_RESERVE   ((size_t)6144 * 1024 * 1024)  // 6 GB virtual reservation window
#define OVERCOMMIT_HEAP_MB  608u                          // newlib malloc + .so load zone

/* ============================ GAME IDENTITY =============================== */

// Fruit Ninja Classic + ships the engine as the standard modern Unity trio; libmain.so
// dlopens libunity.so which dlopens libil2cpp.so. (No libcrx/MVGL here -- this
// is a normal IL2CPP game, so main.c loads libmain/libunity/libil2cpp directly
// and these SO_NAME macros are unused, kept only for parity with the base.)
#define SO_NAME      "libunity.so"
#define SO_CPP_NAME  "libil2cpp.so"

// The SD-card folder holding the .nro + the game files.
#define GAME_FOLDER  "abreloaded"

/* Bump when shipping. Printed at compile time (#pragma message in main.c)
 * and at boot, so a stale source tree is obvious from either the build
 * output or debug.log. */
#define ABR_SRC_REV   "abr-r17-r9-keyboard-deferred"

/* ---- Android package name -- VERIFY THIS AGAINST YOUR APK -----------------
 * Returned by our fake getPackageName(). Unity surfaces it as
 * Application.identifier, and game code (and any SDK that keys off it) can
 * read it.
 *
 * NOTE: the PvZ tree inherited Zookeeper's "jp.kiteretsu.zookeeper_dx" here and
 * never changed it -- both of those ports shipped reporting the WRONG package
 * name. Do not repeat that.
 *
 * The package name is not present in libunity/libil2cpp; it lives in the APK's
 * AndroidManifest.xml. This value is NOT a placeholder: it was read directly
 * out of the binary AndroidManifest.xml string pool of the Angry Birds
 * Reloaded APK this port targets, alongside the launcher activity
 * (com.unity3d.player.UnityPlayerActivity) and versionName 2.2.16218.
 *
 * It matters: getPackageName() feeds Application.identifier and the Context
 * path getters, so a wrong value sends persistentDataPath somewhere the game
 * does not expect.                                                          */
#define GAME_PACKAGE "com.rovio.reloadedport"

/* ---- split-asset auto-join (nx_splitjoin.c) ------------------------------
 * This build ships some assets as 1 MiB .split0/.split1/... parts, which Unity
 * reassembles in Java on a phone. We have no Java, so the loader joins them on
 * the SD card at first boot. Set to 1 to delete the .splitN parts once a join
 * has been verified -- saves ~8 MB, but means re-copying from the APK if you
 * ever want them back. Off by default: we do not delete user data uninvited. */
#define JOIN_DELETE_PARTS 0

/* Diagnostic: trace futex WAIT/WAKE in the contended allocator region to
 * locate a lost wake. Verbose; enabled for this bring-up build only. */
#define ABR_FUTEX_TRACE 0

#define CONFIG_NAME "config.txt"
#define LOG_NAME    "sdmc:/switch/" GAME_FOLDER "/debug.log"

// Returned for getenv("HOME")/getpwuid()->pw_dir. Point it at the (writable)
// game data root instead of letting the engine deref a NULL passwd.
#define GAME_HOME   "sdmc:/switch/" GAME_FOLDER
#ifndef DATA_ROOT
#define DATA_ROOT   "sdmc:/switch/" GAME_FOLDER
#endif

// Flip to 1 and rebuild to get file logging (debug.log) for on-hardware debugging.
/* File logging.
 *
 *   1 = ON. debug.log is written normally: boot trace, per-frame
 *           diagnostics, [gc]/[mem] notes and the [xd] crash dump.
 *
 *   0 = ABSOLUTE SILENCE. debug.log is never created -- not by a crash, not by
 *       a "note", not at all. Every logging entry point compiles to an empty
 *       stub, so there is no file, no formatting cost and no SD traffic.
 *
 * Note that 0 does NOT disable the watchdog thread: it is also the escape hatch
 * that undoes a wedged GC stop-the-world (round 101), so it always runs. */
#define DEBUG_LOG 0   /* Normal-play build: no log formatting or SD-card writes.
                       * Set to 1 temporarily when collecting a diagnostic log. */

/* Keep the audio timeline tied to visible progress. Unity performs scene and
 * cutscene setup inside nativeRender(); on Switch that call can block for
 * seconds while the SDL/OpenSL callback otherwise keeps consuming FMOD audio.
 * After this many milliseconds in one render call, output silence WITHOUT
 * consuming queued samples or firing completion callbacks. When nativeRender
 * returns, playback resumes at the exact sample at which the picture stalled.
 * 75 ms is above an ordinary 30/60 fps frame but short enough to keep lip-sync
 * error below a tenth of a second when a cutscene has a multi-second startup. */
#define ABR_AV_SYNC_STALL_MS 75

/* GC stop-the-world (round 100).
 *
 *   1 = CORRECT. Mutator threads are really paused while the collector marks.
 *       This is what a garbage collector requires, and without it the mark loop
 *       can read a half-published object and fault (round 99: klass == 0).
 *       The cost is real: the game's worker threads are stopped for the whole
 *       mark, so a large collection shows up as an occasional frame hitch.
 *
 *   0 = OLD BEHAVIOUR. Ack the suspend without pausing anyone. Smooth, and
 *       wrong -- this is the configuration that crashed ~20% of the time when
 *       starting a new game.
 *
 * Left ON: an occasional hitch is a better failure than a crash. Flip it to 0
 * if you would rather have the old behaviour back. */
#define ABR_GC_STOP_WORLD 1

/* Strip "gc-max-time-slice" from boot.config as it is served to the engine.
 *
 * That key puts Unity's collector in INCREMENTAL mode: marking is split across
 * many short slices with the mutators running in between, and the invariant is
 * held together by write barriers plus a correct stop-the-world for each slice.
 * This port's stop-the-world is not reliable enough for that -- it works for ~32
 * collections in 33 and bails on the rest (round 110) -- and every crash so far
 * has landed in the mark loop reading a klass of 0, which is what a broken
 * incremental invariant looks like.
 *
 * With the key removed the collector runs non-incremental: fewer, larger, atomic
 * collections, with no between-slice invariant to violate. Expect occasional
 * longer pauses in exchange.
 *
 * 0 restores the game's shipped setting. */
#define ABR_GC_NON_INCREMENTAL 1

/* High-volume per-operation traces. These were invaluable for the black-screen /
 * boot-hang triage but are catastrophic for load speed once the game runs: every
 * data.unity3d read/lseek and most mprot calls fflush two lines to the SD card, so
 * a synchronous scene load (~1700 bundle reads) takes minutes instead of seconds
 * and looks like a hang. Keep them OFF for normal play; flip to 1 to re-trace. */
#define TRACE_BUNDLE_IO 0   /* per-read/lseek trace of data.unity3d */
#define TRACE_MPROT     0   /* per-mprotect commit trace */

/* Per-frame / per-asset traces that dominate the log once the game runs:
 * the doFrame proxy line (every frame), [io] open (every asset, ~3x), and
 * the clock-stall beacon. Off = a readable log and far less SD I/O; flip to
 * 1 only to re-trace JNI proxy dispatch or asset open order. */
#define LOG_VERBOSE 0

/* Mutex ownership tracker (self-deadlock / holder naming). It takes a global
 * lock on every pthread_mutex op, which serialises the engine's mutex
 * traffic and reorders acquisition -- fine for a one-off deadlock hunt, but
 * it must be OFF for normal runs or it changes timing enough to hang boot.
 * Flip to 1 only to re-diagnose a mutex deadlock. */
#define MTXOWN_ENABLE 0

/* Force glFinish() before eglSwapBuffers on the first N presents, to work
 * around / localise the first-present hang (mesa blocking in its flush/
 * fence phase). 0 disables. A small number (a few frames) is enough to get
 * past the initial present without serialising steady-state rendering. */
/* round 63: adaptive futex re-poll floor. 250us gives fast recovery of
 * silent-writer waits (the async-load bottleneck) while backoff keeps idle
 * threads cheap. Lower = faster loads but more wakeups. */
#define ABR_FUTEX_HOP_MIN 250000ULL
/* round 70: re-poll CEILING. Handoffs whose wake never arrives directly cost
 * one tick of this, so it sets the load speed. 16ms was the old value and is
 * why loading crawled; 1ms is ~16x faster. Lower = faster loads, more CPU. */
#define ABR_FUTEX_HOP_MAX 1000000ULL
/* round 64: vsync/Choreographer pulse period. 16ms == ~60fps cap; the load
 * is frame-gated so a shorter period renders (and loads) faster. Delta-time
 * is hooked so game speed is unchanged. */
#define ABR_VSYNC_PERIOD_NS 16000000ULL
#define ABR_SWAP_FINISH_N 8

/* ---- libil2cpp hook gates (see patches/patch_sources.py) -----------------
 * libil2cpp is GAME CODE: every offset into it is specific to one build of one
 * game. The PvZ core hardcodes hooks at PvZ's offsets. Both are OFF here because
 * neither was re-derived for Fruit Ninja; turning one on without re-deriving it
 * first will patch unrelated functions. Symptoms and method: PORTING sec 6.    */
#define ABR_HAVE_TIME_HOOKS 0 /* 0: Fruit Ninja offsets, NOT derived for this game -- see nx_patch_abr.h */
#define ABR_FORCE_SPLASH_FINISH 0 /* 0: Fruit Ninja offsets, NOT derived for this game -- see nx_patch_abr.h */
/* round 68: async-load integration budget, ms per frame. Unity default is
 * 4ms (High would be 50). Bigger = faster scene loads, fewer frames during
 * loading. 0 disables the patch. */
#define ABR_PRELOAD_BUDGET_MS 16
/* round 75: NOP UpdatePreloading's early-exit branch so the main thread keeps
 * retrying SingleStep for the whole budget instead of yielding the frame the
 * moment the integrate queue is briefly empty. 0 disables. */
#define ABR_PRELOAD_NO_EARLY_EXIT 1
/* round 69: per-asset "[io] DATA open" trace. Costs an extra fstat plus a
 * log line for each of the ~2200 assets loaded, so keep it off by default. */
#define ABR_TRACE_DATA_IO 0
/* round 79: build+mount the Subway-Surfers-style asset pack. First boot
 * packs assets/bin/Data into one file; later boots mount it. 0 = off. */
/* Angry Birds Reloaded ships NO .splitN parts -- verified against the APK's
 * assets/ tree. The four basenames the Fruit Ninja base scanned for
 * (globalgamemanagers.assets, level8, sharedassets2/3.assets) do not exist in
 * this game at all, so both the join scan and the unjoined-parts check are
 * compiled out rather than left to probe for files that can never appear. */
#define ABR_HAVE_SPLIT_ASSETS 0

#define ABR_ASSET_PACK 0
/* OFF for Angry Birds Reloaded, and this is a SAFETY decision, not a tuning one.
 *
 * With this at 1 the loader, on first boot, builds a single packed archive from
 * DATA_ROOT/assets and then calls nx_rmtree(DATA_ROOT "/assets") -- it DELETES
 * the player's loose asset tree. Fruit Ninja wanted that: it loads ~4200 small
 * resource files and each open is expensive here, so folding them into one pack
 * is a large win.
 *
 * This game is the opposite shape. Its assets are a handful of very large files
 * (data.unity3d ~367 MB, sharedassets0.resource ~84 MB, 26 tutorial MP4s), so
 * packing buys essentially nothing, while the build needs ~520 MB of spare SD
 * space and the rmtree afterwards is destructive and irreversible.
 *
 * asset_pack.h is now included unconditionally by libc_shim.c, so turning this
 * off leaves every asset_pack_* call site compiling and no-opping at runtime. */
#define ABR_BYPASS_UNITY_SPLASH 0 /* 0: Fruit Ninja offsets, NOT derived for this game -- see nx_patch_abr.h */
/* JNI approximation ledger (round 130). Records every JNI call answered by a
 * catch-all -- empty string, NULL object, 0, no-op -- deduped and counted, and
 * marks the ones Unity reads back via ExceptionCheck. Android raises at this
 * boundary and we cannot; this is the half of that which costs nothing. The
 * ledger is reprinted in full on any crash dump, so a fault log now carries
 * "here is everything we faked" instead of needing a separate run.
 * Set to 0 for absolute silence (DEBUG_LOG 0 already suppresses the output). */
#define ABR_JNI_LOUD 1

/* Diagnostic only: poison freed newlib blocks with 0xDE to expose stale reads.
 * This adds a memset to every free and can cause frame hitches during asset and
 * garbage-collection cleanup. Keep disabled for normal play. */
#define ABR_POISON_FREE 0


/* Diagnostic ONLY: call il2cpp_gc_disable() after init so no collection ever
 * runs. Answers "is the corruption premature reclamation?" in one session.
 * Memory grows unbounded -- never ship with this set to 1. */
#define ABR_GC_DISABLE 0

/* The on-device asset-pack payload verifier is GONE (round 142). It ran once,
 * printed "verify OK: payload matches header (b3e645de7a86ae28)", and that is
 * recorded -- it had no business costing a 255 MB blocking read on the boot
 * path. The same check now lives in tools/verify_pack.py, run on a PC. */

#define ABR_HAVE_GC_BRIDGE  1   /* DERIVED for this game -- 4 offsets, each confirmed
                                 * by >=2 independent routes. See nx_patch_abr.h. */
#define ABR_GC_BRIDGE_SIMPLE 1  /* 1 = Papers, Please's four-offset model (ack on the
                                 * thread's behalf). 0 = Fruit Ninja's real
                                 * stop-the-world, which additionally needs
                                 * GC_threads / GC_stop_count / GC_retry_signals --
                                 * NOT derived here, so do not set this to 0. */

extern int screen_width;
extern int screen_height;

/* ----------------------------- Language ----------------------------------
 * NOT INHERITED FROM PvZ -- re-derived for this game, and it works differently.
 *
 * PvZ's build read its locale as a STRING from an AndroidJavaClass, so its
 * config mapped 1/2 onto the literals "en"/"zh" returned by jni_fake's
 * getLanguage(). Fruit Ninja does not do that. Its managed code calls
 *
 *     UnityEngine.Application::get_systemLanguage()
 *
 * which returns Unity's SystemLanguage ENUM. The engine populates that at init
 * from the Java locale, so jni_fake's getLanguage() still feeds it -- but the
 * game compares against an enum, not against our string, so there is no
 * game-specific token to guess.
 *
 * Practical consequence: LANG_AUTO is the only value with confirmed meaning.
 * The overrides below simply force the locale jni_fake reports; verify on
 * hardware which SystemLanguage the engine derives before trusting them.
 *
 * (The binary also carries I18N.CJK/MidEast/Other/Rare/West and RTLTMPro, so
 * the title has broad language coverage including right-to-left. The ar-*
 * tokens visible in libil2cpp are mscorlib CULTURE TABLES from I18N.MidEast,
 * not a list of shipped game languages -- do not read them as one.)        */
#define LANG_AUTO 0   /* follow the Switch system language -- recommended */
#define LANG_EN   1   /* force en */
#define LANG_ZH   2   /* force zh -- retained from the base; UNVERIFIED here */

/* ORIENTATION -- UNRESOLVED, AND YOU MUST CHECK THIS BEFORE FIRST BOOT.
 *
 * PvZ removed the base's TATE/portrait path because that game is landscape-only.
 * We have NOT confirmed Fruit Ninja's orientation: its managed code contains no
 * ScreenOrientation manipulation and no landscape/portrait strings, so the APK's
 * AndroidManifest.xml (android:screenOrientation) is authoritative. Check it.
 *
 *   landscape -> nothing to do; this build matches PvZ and is correct as-is.
 *   portrait  -> you must restore the Zookeeper TATE compositor-rotation path,
 *                AND revisit nx_pointer, which maps the touch panel to render
 *                space with a straight stretch and applies no rotation. Aiming
 *                will be wrong otherwise -- silently, which is the worst kind.
 *
 * The `portrait` knob is retired here only because it is retired upstream; it
 * is NOT a statement that this game is landscape.                          */
typedef struct {
  int handheld_res;   /* render height in handheld mode: 720 or 1080 */
  int docked_res;     /* render height when docked:      720 or 1080 */
} Config;

extern Config config;

int read_config(const char *file);
int write_config(const char *file);

#endif
