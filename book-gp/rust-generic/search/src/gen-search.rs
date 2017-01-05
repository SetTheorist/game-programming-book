
pub struct TimeControl {
  active: bool,          /* TODO: confirm that this is handled correctly... */

  /* Initial time-control */
  max_depth: i32,        /* 0 = unlimited */
  max_nodes: u64,        /* 0 = unlimited */
  increment_cs: i32,     /* increment-per-move, in centi-seconds */
  byoyomi_cs: i32,       /* time per move (not accumulated), in centi-seconds */
  starting_cs: i32,      /* initial time on clock, in centi-seconds */
  moves_per: i32,        /* moves per control period */

  /* Current state at start of move */
  moves_remaining: i32,  /* 0 = sudden death */
  remaining_cs: i32,     /* current time on clock, in centi-seconds */

  /* Working details */
  allocated_cs: i32,     /* time allocated for move, in centi-seconds */
  panic_cs: i32,         /* extra time allocated for move, in centi-seconds */
  force_stop_cs: i32,    /* absolute max time to use */
  start_clock: u64,      /* starting clock time (microseconds from epoch) */
}
impl TimeControl {
  pub fn new() -> TimeControl {
  }
  /*
  int showf(FILE* ff) const {
    int n = 0;
    n += fprintf(ff, "<%i", active);
    n += fprintf(ff, "|%i,%llu,%i,%i,%i,%i", max_depth, max_nodes, increment_cs, byoyomi_cs, starting_cs, moves_per);
    n += fprintf(ff, "|%i,%i", moves_remaining, remaining_cs);
    n += fprintf(ff, "|%i,%i,%i,%llu", allocated_cs, panic_cs, force_stop_cs, start_clock);
    n += fprintf(ff, ">");
    return n;
  }
  */
}

const MAXSEARCHDEPTH: i32 = 64;

  struct Id {
    f: bool,
    base: i32,
    step: i32,
  }
  struct Iid {
    f: bool,
    base: i32,
    step: i32,
  }
  struct Asp {
    f: bool,
    width: i32,
  }
  struct Nmp {
    f: bool,
    cutoff: i32, /* use R1 reduction if depth>cutoff else R2 */
    R1: i32,
    R2: i32,
  }
  struct Futility {
    f: bool,
    f_ext: i32,
  }
  struct Razoring {
    f: bool,
    f_lim: i32,
  }
  struct Lmr {
    f: bool,
    reduction: i32,
  }

struct SearchSettings {
  /* basic search depth */
  depth: i32,    /* in %ge of ply */
  /* quiescence search */
  qdepth: i32,    /* in %ge of ply */
  //void set_depth(int depth_, int qdepth_) { depth = depth_; qdepth = qdepth_; }

  /* iterative deepening */
  id: Id,
  //void set_id(int f_=1, int base_=200, int step_=100) { id.f = f_; id.base = base_; id.step = step_; }

  /* internal iterative deepening */
  iid: Iid,
  //void set_iid(int f_=0, int base_=200, int step_=100) { iid.f = f_; iid.base = base_; iid.step = step_; }

  /* aspiration search */
  asp: Asp,
  //void set_asp(int f_=0, int width_=100) { asp.f = f_; asp.width = width_; }

  /* null move pruning */
  nmp: Nmp,
  //void set_nmp(int f_=0, int cutoff_=600, int R1_=300, int R2_=200) { nmp.f = f_; nmp.cutoff = cutoff_; nmp.R1 = R1_; nmp.R2 = R2_; }

  /* futility */
  futility: Futility,
  //void set_futility(int f_=0, int f_ext_=200) { futility.f = f_; futility.f_ext = f_ext_; }

  /* razoring */
  razoring: Razoring,
  //void set_razoring(int f_=0, int f_lim_=100) { razoring.f = f_; razoring.f_lim = f_lim_; }

  /* late-move reduction */
  lmr: Lmr,
  //void set_lmr(int f_=0, int reduction_=200) { lmr.f = f_; lmr.reduction = reduction_; }

/*
  int show(FILE* f) const {
    fprintf(f, "{");
    fprintf(f, "AB:");
    fprintf(f, "D(%i)", depth);
    fprintf(f, "Q(%i)", qdepth);
    if (id.f) fprintf(f, ",ID(%i+%i)", id.base, id.step);
    if (iid.f) fprintf(f, ",IID(%i+%i)", iid.base, iid.step);
    fprintf(f, ",ASP(%i)", asp.width);
    fprintf(f, ",HH");
    fprintf(f, ",QHH");
    fprintf(f, ",TT");
    fprintf(f, ",QTT");
    if (lmr.f) fprintf(f, ",LMR(%i)", lmr.reduction);
    if (futility.f) fprintf(f, ",FUT");
    if (futility.f_ext) fprintf(f, ",EXTFUT");
    if (razoring.f) fprintf(f, ",RAZ");
    if (razoring.f_lim) fprintf(f, ",LIMRAZ");
    if (nmp.f) fprintf(f, ",NMP(%i/%i[%i])", nmp.R1, nmp.R2, nmp.cutoff);
    fprintf(f, "}");
    return 0;
  }
  */
}
impl SearchSettings {
}

const TTNUM : i32 = (16*1024*1024);
const TTMASK : i32 = (TTNUM-1);
const QTTNUM : i32 = (16*1024*1024);
const QTTMASK : i32 = (QTTNUM-1);

enum TTFlags {
  tt_invalid=0,
  tt_exact=1,
  tt_upper=2,
  tt_lower=4,
  tt_have_move=16,
}

type Hash = u64;

#[derive(Clone,Copy,Debug,Default)]
struct TTEntry<Move:Default,Eval> {
  lock: Hash,
  value: Eval,
  best_move: Move,
  depth: u16,
  flags: u16,
}


trait GameTrait {
    type Eval: Clone+Copy+Default+Ord+PartialOrd;
    type Move: Clone+Copy+Default;
}

#[derive(Debug,Default)]
struct Stats {
  hit: i64,
  false_hit: i64,
  deep: i64,
  shallow: i64,
  used: i64,
  used_exact: i64,
  used_upper: i64,
  used_lower: i64,
  cutoff: i64,
}
impl Stats {
    fn new() -> Stats { Stats::default() }
    fn reset(&mut self) {
        *self = Stats::new();
    }
}

struct TranspositionTable<Game:GameTrait> {
  stats: Stats,
  num: usize,
  entries: Vec<TTEntry<Game::Move,Game::Eval>>,
}
impl<Game:GameTrait> TranspositionTable<Game> {
    pub fn new(num: usize) -> Self {
        TranspositionTable {
            stats: Stats::new(),
            num: num,
            entries: vec![TTEntry::default(); num],
        }
    }
    pub fn reset_stats(&mut self) {
        self.stats.reset();
    }
    #[inline]
    fn ttloc(&self, hash: Hash) -> usize { (hash as usize) % self.num }
    #[inline]
    pub fn retrieve(&mut self, hash: Hash) -> Option<TTEntry<Game::Move,Game::Eval>> {
        let l = self.ttloc(hash);
        if self.entries[l].flags == TTFlags::tt_invalid as u16 { return None; }
        if self.entries[l].lock != hash { self.stats.false_hit+=1; return None; }
        Some(self.entries[l])
    }
    pub fn store(&mut self, hash: Hash, score: Game::Eval, m: Game::Move, flags: u16, depth: u16) {
        /* TODO: investigate other replacement schemes */
        /* most-recent replacement scheme: */
        let l = self.ttloc(hash);
        self.entries[l].lock = hash;
        self.entries[l].value = score;
        self.entries[l].best_move = m;
        self.entries[l].depth = depth;
        self.entries[l].flags = flags;
    }
}

/*

#if HISTORY_HEURISTIC
// The alternative (color/piece/to) doesn't seem any better (in fact, a bit worse)
#define HH_TO_FROM

template <typename Game>
struct history_table {
  typedef typename Game::evalt evalt;
  typedef typename Game::movet movet;

#ifdef  HH_TO_FROM
  ui32 hist_heur[Game::MAXBOARDI][Game::MAXBOARDI];
#else
  ui32 hist_heur[2][Game::num_piece][Game::MAXBOARDI];
#endif

  void reset()
  {
    memset((void*)hist_heur, '\x00', sizeof(hist_heur));
  }

  void store(movet m, int depth)
  {
    if (depth < 100) return;
    if (m.is_special)
    {
      if (m.special.movetype)
      {
        const int to1 = m.special.from + dir_map[m.special.to1];
        const int to2 = to1 + dir_map[m.special.to2];
#ifdef  HH_TO_FROM
        hist_heur[m.special.from][to1] += 1<<((depth-100)/100)/2;
        hist_heur[m.special.from][to2] += 1<<((depth-100)/100)/2;
#else
        hist_heur[m.special.side_is_white][m.special.piece][to1] += 1<<((depth-100)/100)/2;
        hist_heur[m.special.side_is_white][m.special.piece][to2] += 1<<((depth-100)/100)/2;
#endif
      }
    }
    else
    {
#ifdef  HH_TO_FROM
      hist_heur[m.from][m.to] += 1<<((depth-100)/100);
#else
      hist_heur[m.side_is_white][m.piece][m.to] += 1<<((depth-100)/100);
#endif
    }
  }

  ui32 retrieve(movet m)
  {
    if (m.is_special)
    {
      if (m.special.movetype)
      {
        const int to1 = m.special.from + dir_map[m.special.to1];
        const int to2 = to1 + dir_map[m.special.to2];
#ifdef  HH_TO_FROM
        return hist_heur[m.special.from][to1] + hist_heur[m.special.from][to2];
#else
        return hist_heur[m.side_is_white][m.piece][to1] + hist_heur[m.side_is_white][m.piece][to2];
#endif
      }
      else
      {
        return 0;
      }
    }
    else
    {
#ifdef  HH_TO_FROM
      return hist_heur[m.from][m.to];
#else
      return hist_heur[m.side_is_white][m.piece][m.to];
#endif
    }
  }
};
#endif

enum node_type {
  pv_node   =  0,
  all_node  = -1,
  cut_node  = +1
};

template<typename Game>
struct searcher {
  typedef typename Game::evalt evalt;
  typedef typename Game::movet movet;
  typedef typename Game::boardt boardt;

  search_settings settings;

  time_control tc;

  /*volatile*/ int stop_search;
  ui64 search_start_time;
  ui64 search_stop_time;
  void periodic_check()
  {
    if (tc.active)
    {
      const ui64 t = read_clock();
      if (t >= search_stop_time)
      {
        sendlog("*** Enforcing hard stop on time [%'.2fs]***\n", (t - search_start_time)*1e-6);
        stop_search = 1;
      }
    }
  }

  ui64 nodes_searched;
  ui64 nodes_searched_depth[MAXSEARCHDEPTH+1];
  ui64 nodes_evaluated;
  ui64 nodes_evaluated_depth[MAXSEARCHDEPTH+1];

  ui64 qnodes_searched;
  ui64 qnodes_searched_depth[MAXSEARCHDEPTH+1];
  ui64 qnodes_evaluated;
  ui64 qnodes_evaluated_depth[MAXSEARCHDEPTH+1];

  int max_ply_reached;
  int max_qply_reached;

  transposition_table<Game> tt;
  transposition_table<Game> qtt;

  int pv_length[MAXSEARCHDEPTH];
  movet pv[MAXSEARCHDEPTH][MAXSEARCHDEPTH];

  //movet qkillers[MAXSEARCHDEPTH][2];
#if HISTORY_HEURISTIC
  history_table history;
  history_table qhistory;
#endif

  searcher(int depth, int qdepth, int ttnum, int qttnum) : tt(ttnum), qtt(qttnum) {
    memset((void*)&tc, '\x00', sizeof(tc));
    memset((void*)&settings, '\x00', sizeof(settings));
    settings.depth = depth;
    settings.qdepth = qdepth;
  }
  ~searcher()
  { }
  // default copy constructor & assignment, but they are expensive operations!

  template <typename evaluator>
  evalt search(boardt* b, const evaluator* e, movet* pv, int* pv_length);

  FILE* logfile() { return stderr; }

protected:
  template <typename evaluator>
  evalt
  search_alphabeta_quiesce(
     boardt* b,
     const evaluator* e,
     int qdepth,
     evalt alpha,
     evalt beta,
     int ply,
     node_type type
     );

  template <typename evaluator>
  evalt
  search_alphabeta_general(
      boardt* b,
      const evaluator* e,
      int depth,
      evalt alpha,
      evalt beta,
      int ply,
      node_type type
      );

  template <typename evaluator>
  evalt
  search_alphabeta(
      boardt* b,
      const evaluator* e,
      movet* out_pv,
      int* out_pv_length
      );

  inline void update_pv(int ply, movet m) {
    memcpy(&pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(*pv[ply])*pv_length[ply+1]);
    pv[ply][ply] = m;
    pv_length[ply] = pv_length[ply+1]+1;
  }

  int early_abort_on_time(ui64 last_iter_time,
      evalt best_val, int iter, evalt evals_by_iter[],
      int num_last_iter_pv, movet last_iter_pv[]);

  int another_iteration(
      ui64 last_iter_time,
      int iter,
      evalt evals_by_iter[]);
};

*/

enum make_move_flags {
  ok_move=0, invalid_move=1, same_side_moves_again=2,
}

/* API assumed for game, boardt, movet, evalt, evaluator:
 *
 * game
 *  type boardt
 *  type movet
 *  type evalt
 *
 * game::boardt:
 *  int gen_moves(movet ml[]);
 *  make_move_flags make_move(movet m);
 *  unmake_move();
 *  hasht hash();
 *  static int MAXNMOVES;
 *  bool terminal();
 *  bool in_check(side);
 *
 * movet:
 *  // value type union with int .i
 *  bool valid();
 *  bool null();
 *  static movet nullmove;
 *
 * evalt:
 *  // value type, orderable (either integer or floating-point typically)
 *  static evalt W_INF;
 *  static evalt B_INF;
 *  static evalt STALEMATE_RELATIVE;
 *  static evalt W_WIN_IN_N(int n);
 *  static evalt B_WIN_IN_N(int n);
 *  int PLIES_TO_WIN(evalt);
 *
 * evaluator:
 *  evalt evaluate_relative(boardt* b, int ply, evalt alpha, evalt beta);
 *
 */

/* ******************************************************************************** */
/* IMPLEMENTATION OF TEMPLATES, ETC */
/* ******************************************************************************** */

//#include "gen-search.cpp"

/* ******************************************************************************** */
/* ******************************************************************************** */
