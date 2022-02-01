;; list of variables.
(enum VarKind :a :b :c :d :e :f :g :h :i)
(data Xpr
	(:expr-int i4)
	(:expr-var var-kinds))

(data Inst
	(:inst-assign VarKind Expr)
	(:inst-add VarKind Expr Expr)
	(:inst-if Expr Inst Inst)
	(:inst-while Expr Inst)
	(:inst-bb (List Inst i4)) ;; max size upto 2^4.

(data Lattice
	(:lattice-bot)
	(:lattice-int i4)
	(:lattice-top))

(data Env
	(:lattice-map (Map VarKind Lattice)))

def expr-eval (XprInt i) (env: Env) -> Lattice {
	Lattice-int(i)
}

def expr-eval (Xpr-var v) (env: Env) -> Lattice {
	l? := e @ v;
	l? {
		Just(l): x;
		Nothing: Lattice-bot;
	};
}

def const-prop (InstAssign lhs rhs) (env: Env) -> Env {
	lattice-val := expr-eval rhs env;
	env @ lhs = lattice-val;
}

def lattice-add (LatticeInt i) (LatticeInt j) -> LatticeInt  {
	out := i + j;
	LatticeInt(out);
}

def lattice-add (LatticeTop) (_) -> LatticeInt { LatticeTop }
def lattice-add (_) (LatticeTop) -> LatticeInt { LatticeTop }
def lattice-add (_) (_) -> LatticeInt { LatticeBot }

def const-prop (InstAdd var lhs rhs) (env: Env) -> Either(Inst, Env) {
	lhs-val := expr-eval lhs env;
	rhs-val := expr-eval rhs env;
	sum-val := lattice-add lhs-val rhs-val;
	sum-val {
		LatticeInt(i): {
			out-inst := InstAssign var i;
			out-inst
		}
		LatticeTop: { }
	}

def const-prop (InstIf cond lhs rhs) (env: Env) -> Env {
	
}

def const-prop-fix (i: Inst) (e: Env) -> Env {
	
}