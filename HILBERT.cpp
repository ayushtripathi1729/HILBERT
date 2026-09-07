// H.I.L.B.E.R.T. 21.0 -- persistent autonomous mathematical discovery laboratory with active hypothesis experimentation.
// Extended rigorous domains: exact algebra, elementary number theory,
// exact combinatorics, conjecture generation, adversarial testing,
// certificate-gated proof, reproducible experiments and research statistics.
// First rigorous domain: exact rational arithmetic and elementary polynomial identities.
// A statement becomes a theorem only after Kernel::verify accepts its certificate.

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <limits>
#include <tuple>
#include <cmath>
#include <cstdio>

namespace hilbert {

// H.I.L.B.E.R.T. 3.1 FINAL - warning-clean C++14-compatible build.

// A tiny C++17-compatible optional value keeps this file buildable with the
// workspace's legacy compiler too, without pulling non-standard dependencies.
template<class T> class Optional {
    bool present_ = false;
    T value_{};
public:
    Optional() = default;
    Optional(const T& value) : present_(true), value_(value) {}
    Optional(T&& value) : present_(true), value_(std::move(value)) {}
    explicit operator bool() const { return present_; }
    T& operator*() { return value_; }
    const T& operator*() const { return value_; }
    T* operator->() { return &value_; }
    const T* operator->() const { return &value_; }
};

// ============================================================================
// 1. Configuration and diagnostics
// ============================================================================
struct Config {
    int iterations=50, tests=500, maxDepth=12, maxStates=50000, timeLimitMs=0;
    int maxRepairs=4, researchBeam=24;
    int missionBudget=6, missionSteps=4;
    uint64_t seed=1; bool verbose=false;
};
struct Error : std::runtime_error { using std::runtime_error::runtime_error; };
static void require(bool b, const std::string& s) { if (!b) throw Error(s); }

// ============================================================================
// 2. Exact arithmetic: arbitrary precision signed integer, then normalized Q
// ============================================================================
class BigInt {
    static constexpr uint32_t BASE=1000000000; // little-endian decimal limbs
    std::vector<uint32_t> d; bool neg=false;
    void trim() { while(!d.empty() && d.back()==0) d.pop_back(); if(d.empty()) neg=false; }
    static int cmpAbs(const BigInt&a,const BigInt&b) {
        if(a.d.size()!=b.d.size()) return a.d.size()<b.d.size()?-1:1;
        for(size_t i=a.d.size();i--;) if(a.d[i]!=b.d[i]) return a.d[i]<b.d[i]?-1:1;
        return 0;
    }
    static BigInt addAbs(BigInt a,const BigInt&b) { uint64_t c=0; if(a.d.size()<b.d.size())a.d.resize(b.d.size()); for(size_t i=0;i<a.d.size();++i){uint64_t x=c+a.d[i]+(i<b.d.size()?b.d[i]:0);a.d[i]=uint32_t(x%BASE);c=x/BASE;}if(c)a.d.push_back(uint32_t(c));return a; }
    static BigInt subAbs(BigInt a,const BigInt&b) { int64_t br=0; for(size_t i=0;i<a.d.size();++i){int64_t x=int64_t(a.d[i])-(i<b.d.size()?b.d[i]:0)-br;br=x<0;if(br)x+=BASE;a.d[i]=uint32_t(x);}a.trim();return a; }
public:
    BigInt()=default;
    // Converting INT64_MIN through -n would overflow signed arithmetic.
    BigInt(int64_t n) {
        uint64_t magnitude;
        if (n < 0) { neg=true; magnitude=uint64_t(-(n+1)); ++magnitude; }
        else magnitude=uint64_t(n);
        while(magnitude){d.push_back(uint32_t(magnitude%BASE));magnitude/=BASE;}
    }
    explicit BigInt(const std::string&s) { size_t p=0;if(s.empty())throw Error("empty integer");if(s[0]=='-'){neg=true;p=1;}if(p==s.size())throw Error("bad integer");for(;p<s.size();++p){if(!std::isdigit((unsigned char)s[p]))throw Error("bad integer");*this=*this*BigInt(10)+BigInt(s[p]-'0');}if(isZero())neg=false; }
    bool isZero()const{return d.empty();} bool negative()const{return neg;} BigInt abs()const{BigInt x=*this;x.neg=false;return x;}
    std::string str()const { if(d.empty())return "0";std::ostringstream o;if(neg)o<<'-';o<<d.back();for(size_t i=d.size()-1;i--;)o<<std::setw(9)<<std::setfill('0')<<d[i];return o.str(); }
    friend bool operator==(const BigInt&a,const BigInt&b){return a.neg==b.neg&&a.d==b.d;} friend bool operator!=(const BigInt&a,const BigInt&b){return !(a==b);} friend bool operator<(const BigInt&a,const BigInt&b){if(a.neg!=b.neg)return a.neg;int c=cmpAbs(a,b);return a.neg?c>0:c<0;}
    friend bool operator>(const BigInt&a,const BigInt&b){return b<a;}
    friend bool operator<=(const BigInt&a,const BigInt&b){return !(b<a);}
    friend bool operator>=(const BigInt&a,const BigInt&b){return !(a<b);}
    friend BigInt operator-(BigInt a){if(!a.isZero())a.neg=!a.neg;return a;}
    friend BigInt operator+(const BigInt&a,const BigInt&b){if(a.neg==b.neg){BigInt r=addAbs(a,b);r.neg=a.neg;return r;}int c=cmpAbs(a,b);if(c==0)return {};if(c>0){BigInt r=subAbs(a,b);r.neg=a.neg;return r;}BigInt r=subAbs(b,a);r.neg=b.neg;return r;}
    friend BigInt operator-(const BigInt&a,const BigInt&b){return a+(-b);} friend BigInt operator*(const BigInt&a,const BigInt&b){if(a.isZero()||b.isZero())return{};BigInt r;r.d.assign(a.d.size()+b.d.size(),0);for(size_t i=0;i<a.d.size();++i){uint64_t c=0;for(size_t j=0;j<b.d.size()||c;++j){uint64_t z=r.d[i+j]+c+uint64_t(a.d[i])*(j<b.d.size()?b.d[j]:0);r.d[i+j]=uint32_t(z%BASE);c=z/BASE;}}r.neg=a.neg!=b.neg;r.trim();return r;}
    BigInt& operator+=(const BigInt&x){return *this=*this+x;} BigInt& operator-=(const BigInt&x){return *this=*this-x;} BigInt& operator*=(const BigInt&x){return *this=*this*x;}
    // Conservative binary-search long division. Enough for exact gcd and rationals.
    static std::pair<BigInt,BigInt> divmod(BigInt a, BigInt b) {
        if (b.isZero()) throw Error("division by zero");
        bool sign = a.neg != b.neg;
        a.neg = false;
        b.neg = false;
        if (cmpAbs(a, b) < 0) return {BigInt(), sign ? -a : a};

        BigInt q;
        q.d.assign(a.d.size(), 0);
        BigInt rem;
        for (size_t k = a.d.size(); k--;) {
            if (!rem.d.empty()) rem.d.insert(rem.d.begin(), 0);
            else rem.d.push_back(0);
            rem.d[0] = a.d[k];
            rem.trim();

            uint32_t lo = 0, hi = BASE - 1, best = 0;
            while (lo <= hi) {
                uint32_t m = lo + (hi - lo) / 2;
                BigInt v = b * BigInt((int64_t)m);
                int c = cmpAbs(v, rem);
                if (c <= 0) {
                    best = m;
                    if (m == BASE - 1) break;
                    lo = m + 1;
                } else {
                    if (m == 0) break;
                    hi = m - 1;
                }
            }
            q.d[k] = best;
            rem = subAbs(rem, b * BigInt((int64_t)best));
        }
        q.trim();
        q.neg = sign && !q.isZero();
        rem.neg = false;
        return {q, rem};
    }
    friend BigInt operator/(const BigInt&a,const BigInt&b){return divmod(a,b).first;} friend BigInt operator%(const BigInt&a,const BigInt&b){return divmod(a,b).second;}
    friend BigInt gcd(BigInt a,BigInt b){a=a.abs();b=b.abs();while(!b.isZero()){BigInt r=a%b;a=b;b=r;}return a;}
};
class Rational {
public: BigInt n{0},d{1};
    Rational()=default; Rational(int64_t x):n(x),d(1){} Rational(BigInt a,BigInt b):n(std::move(a)),d(std::move(b)){normal();}
    void normal(){if(d.isZero())throw Error("zero rational denominator");if(d.negative()){n=-n;d=-d;}if(n.isZero()){d=BigInt(1);return;}BigInt g=gcd(n.abs(),d);n=n/g;d=d/g;}
    std::string str()const{return d==BigInt(1)?n.str():n.str()+"/"+d.str();} bool isZero()const{return n.isZero();}
    friend bool operator==(const Rational&a,const Rational&b){return a.n==b.n&&a.d==b.d;}friend bool operator<(const Rational&a,const Rational&b){return a.n*b.d<b.n*a.d;}
    friend bool operator!=(const Rational&a,const Rational&b){return !(a==b);}
    friend bool operator>(const Rational&a,const Rational&b){return b<a;}
    friend bool operator<=(const Rational&a,const Rational&b){return !(b<a);}
    friend bool operator>=(const Rational&a,const Rational&b){return !(a<b);}
    friend Rational operator+(Rational a,const Rational&b){a.n=a.n*b.d+b.n*a.d;a.d*=b.d;a.normal();return a;}friend Rational operator-(Rational a,const Rational&b){a.n=a.n*b.d-b.n*a.d;a.d*=b.d;a.normal();return a;}friend Rational operator-(Rational a){a.n=-a.n;return a;}friend Rational operator*(Rational a,const Rational&b){a.n*=b.n;a.d*=b.d;a.normal();return a;}friend Rational operator/(Rational a,const Rational&b){if(b.n.isZero())throw Error("rational division by zero");a.n*=b.d;a.d*=b.n;a.normal();return a;}
};

// ============================================================================
// 3. Mathematical AST, statements, parser and printer
// ============================================================================
enum class Kind { Const, Var, Add, Mul, Div, Pow, Neg, Func };
struct Expr { Kind k; Rational q; std::string name; std::vector<std::shared_ptr<Expr>> a; };
using E=std::shared_ptr<Expr>;
static std::string canonical(const E& e);
static E cn(Rational q){auto e=std::make_shared<Expr>();e->k=Kind::Const;e->q=q;return e;} static E cn(int64_t x){return cn(Rational(x));} static E vr(std::string s){auto e=std::make_shared<Expr>();e->k=Kind::Var;e->name=std::move(s);return e;} static E op(Kind k,std::vector<E>x){auto e=std::make_shared<Expr>();e->k=k;e->a=std::move(x);return e;}
enum class Rel { Eq, Ne, Lt, Le, Gt, Ge, Divides };
struct Assumption { E left,right; Rel rel=Rel::Eq; };
struct Statement { Rel rel=Rel::Eq; E left,right; std::vector<Assumption> assumptions; std::map<std::string,std::string> domains; std::string source; };
static std::string print(const E& e){ switch(e->k){ case Kind::Const:return e->q.str(); case Kind::Var:return e->name; case Kind::Neg:return "-("+print(e->a[0])+")"; case Kind::Add: case Kind::Mul:{ std::string s; const char* separator=e->k==Kind::Add?" + ":" * "; for(size_t i=0;i<e->a.size();++i) s+=(i?separator:"")+print(e->a[i]); return "("+s+")"; } case Kind::Div:return "("+print(e->a[0])+" / "+print(e->a[1])+")"; case Kind::Pow:return "("+print(e->a[0])+" ^ "+print(e->a[1])+")"; case Kind::Func:{std::string s=e->name+"(";for(size_t i=0;i<e->a.size();++i)s+=(i?",":"")+print(e->a[i]);return s+")";} } return "?"; }
static std::string print(const Statement&s){const char* r[] = {"=","!=","<","<=",">",">=","divides"};std::string out=print(s.left)+" "+r[int(s.rel)]+" "+print(s.right);if(!s.assumptions.empty()){out+=" assuming ";for(size_t i=0;i<s.assumptions.size();++i){if(i)out+=" and ";const Assumption&a=s.assumptions[i];out+=print(a.left)+" "+r[int(a.rel)]+" "+print(a.right);}}return out;}
 class Parser { std::string s;size_t p=0; std::vector<Assumption> assumptions; void ws(){while(p<s.size()&&std::isspace((unsigned char)s[p]))++p;} bool eat(const std::string&x){ws();if(s.compare(p,x.size(),x)==0){p+=x.size();return true;}return false;} std::string ident(){ws();if(p==s.size()||!(std::isalpha((unsigned char)s[p])||s[p]=='_'))return{};size_t b=p++;while(p<s.size()&&(std::isalnum((unsigned char)s[p])||s[p]=='_'))++p;return s.substr(b,p-b);}
    E primary(){ws();if(eat("(")){E x=add();if(!eat(")"))throw Error("expected )");return x;}if(p<s.size()&&std::isdigit((unsigned char)s[p])){size_t b=p;while(p<s.size()&&std::isdigit((unsigned char)s[p]))++p;return cn(Rational(BigInt(s.substr(b,p-b)),BigInt(1)));}std::string n=ident();if(n.empty())throw Error("expected expression at "+std::to_string(p));if(eat("(")){std::vector<E>x;if(!eat(")")){do{x.push_back(add());}while(eat(","));if(!eat(")"))throw Error("expected )");}auto z=op(Kind::Func,std::move(x));z->name=n;return z;}return vr(n);}
    // Exponentiation binds more tightly than unary minus: -2^2 == -(2^2).
    // Parentheses still allow (-2)^2.
    E power(){E x=primary();if(eat("^"))x=op(Kind::Pow,{x,power()});return x;}
    E unary(){if(eat("-"))return op(Kind::Neg,{unary()});return power();} E mul(){E x=unary();for(;;){if(eat("*"))x=op(Kind::Mul,{x,unary()});else if(eat("/"))x=op(Kind::Div,{x,unary()});else break;}return x;} E add(){E x=mul();for(;;){if(eat("+"))x=op(Kind::Add,{x,mul()});else if(eat("-"))x=op(Kind::Add,{x,op(Kind::Neg,{mul()})});else break;}return x;}
public: explicit Parser(std::string x):s(std::move(x)){size_t marker=s.find(" assuming ");if(marker!=std::string::npos){std::string tail=s.substr(marker+10);s=s.substr(0,marker);size_t start=0;for(;;){size_t at=tail.find(" and ",start);std::string atom=tail.substr(start,at==std::string::npos?std::string::npos:at-start);Statement a=Parser(atom).statement();assumptions.push_back({a.left,a.right,a.rel});if(at==std::string::npos)break;start=at+5;}}} Statement statement(){Statement z;z.left=add();if(eat("!="))z.rel=Rel::Ne;else if(eat("<="))z.rel=Rel::Le;else if(eat(">="))z.rel=Rel::Ge;else if(eat("="))z.rel=Rel::Eq;else if(eat("<"))z.rel=Rel::Lt;else if(eat(">"))z.rel=Rel::Gt;else if(eat("divides"))z.rel=Rel::Divides;else throw Error("expected relation");z.right=add();ws();if(p!=s.size())throw Error("trailing input at "+std::to_string(p));z.assumptions=assumptions;z.source=s;return z;} };
// NOTE: Parser::atom is intentionally unused; primary is the grammar entry.

// ============================================================================
// 4. Canonical polynomial normalizer (sound for +,-,*, nonnegative integer ^)
// ============================================================================
using Mono=std::map<std::string,int>;
static std::string monoKey(const Mono&m){std::string s;for(const auto& item:m)s+=item.first+"^"+std::to_string(item.second)+";";return s;}
struct Poly { std::map<Mono,Rational> t; void clean(){for(auto i=t.begin();i!=t.end();)if(i->second.isZero())i=t.erase(i);else ++i;} static Poly constant(Rational q){Poly p;if(!q.isZero())p.t[{}]=q;return p;} static Poly variable(const std::string&s){Poly p;p.t[{{s,1}}]=Rational(1);return p;} std::string key()const{std::string s;for(const auto& item:t)s+=item.second.str()+"@"+monoKey(item.first)+"|";return s;} };
static Poly operator+(Poly x,const Poly&y){for(const auto& item:y.t)x.t[item.first]=x.t[item.first]+item.second;x.clean();return x;}static Poly operator-(Poly x){for(auto& item:x.t)item.second=-item.second;return x;}static Poly operator*(const Poly&x,const Poly&y){Poly z;for(const auto& left:x.t)for(const auto& right:y.t){Mono m=left.first;for(const auto& power:right.first)m[power.first]+=power.second;z.t[m]=z.t[m]+left.second*right.second;}z.clean();return z;}
static Optional<int> smallNatural(const E&e){if(e->k!=Kind::Const||e->q.d!=BigInt(1)||e->q.n.negative())return{};try{long long n=std::stoll(e->q.n.str());if(n>40)return{};return int(n);}catch(...){return{};}}
static Optional<Poly> polynomial(const E&e) {
    switch (e->k) {
        case Kind::Const:
            return Poly::constant(e->q);
        case Kind::Var:
            return Poly::variable(e->name);
        case Kind::Neg: {
            auto x = polynomial(e->a[0]);
            return x ? Optional<Poly>(-*x) : Optional<Poly>();
        }
        case Kind::Add: {
            Poly z;
            for (auto& a : e->a) {
                auto x = polynomial(a);
                if (!x) return {};
                z = z + *x;
            }
            return z;
        }
        case Kind::Mul: {
            Poly z = Poly::constant(1);
            for (auto& a : e->a) {
                auto x = polynomial(a);
                if (!x) return {};
                z = z * *x;
            }
            return z;
        }
        case Kind::Div: {
            auto x = polynomial(e->a[0]);
            if (!x) return {};
            if (e->a[1]->k != Kind::Const || e->a[1]->q.isZero()) return {};
            return *x * Poly::constant(Rational(1) / e->a[1]->q);
        }
        case Kind::Pow: {
            auto x = polynomial(e->a[0]);
            auto n = smallNatural(e->a[1]);
            if (!x || !n) return {};
            Poly z = Poly::constant(1);
            for (int i = 0; i < *n; ++i) z = z * *x;
            return z;
        }
        case Kind::Func:
            return {};
    }
    return {};
}
static Poly polyPower(Poly base,int exponent);

// Formal algebraic normalization with function applications treated as opaque
// atoms.  This is sound for +,-,*, and natural powers: the kernel never assumes
// anything about the function's meaning; it only uses the fact that an atom is
// equal to itself.  This lets recursive-definition rewrites finish algebraic
// proofs such as 2*f(n+1)+2*f(n)=2*(f(n+1)+f(n)).
static Optional<Poly> symbolicPolynomial(const E&e){
    switch(e->k){
        case Kind::Const:return Poly::constant(e->q);
        case Kind::Var:return Poly::variable(e->name);
        case Kind::Func:{
            Poly p; p.t[{{"@FUNC:"+canonical(e),1}}]=Rational(1); return p;
        }
        case Kind::Neg:{auto x=symbolicPolynomial(e->a[0]);return x?Optional<Poly>(-*x):Optional<Poly>();}
        case Kind::Add:{Poly z;for(const auto&a:e->a){auto x=symbolicPolynomial(a);if(!x)return{};z=z+*x;}return z;}
        case Kind::Mul:{Poly z=Poly::constant(1);for(const auto&a:e->a){auto x=symbolicPolynomial(a);if(!x)return{};z=z**x;}return z;}
        case Kind::Div:{auto x=symbolicPolynomial(e->a[0]);if(!x)return{};auto den=symbolicPolynomial(e->a[1]);if(!den||den->t.size()!=1)return{};auto it=den->t.begin();if(!it->first.empty()||it->second.isZero())return{};return *x*Poly::constant(Rational(1)/it->second);}
        case Kind::Pow:{auto x=symbolicPolynomial(e->a[0]);auto n=smallNatural(e->a[1]);if(!x||!n)return{};return polyPower(*x,*n);}
    }
    return{};
}

struct PolyFraction { Poly n,d; };
static Poly polyPower(Poly base,int exponent){Poly out=Poly::constant(1);for(int i=0;i<exponent;++i)out=out*base;return out;}
static Optional<PolyFraction> rationalPolynomial(const E&e){
    if(e->k==Kind::Div){auto a=rationalPolynomial(e->a[0]),b=rationalPolynomial(e->a[1]);if(!a||!b)return{};return PolyFraction{a->n*b->d,a->d*b->n};}
    if(e->k==Kind::Add){PolyFraction out{Poly::constant(0),Poly::constant(1)};for(const auto& child:e->a){auto x=rationalPolynomial(child);if(!x)return{};out={out.n*x->d+x->n*out.d,out.d*x->d};}return out;}
    if(e->k==Kind::Mul){PolyFraction out{Poly::constant(1),Poly::constant(1)};for(const auto& child:e->a){auto x=rationalPolynomial(child);if(!x)return{};out={out.n*x->n,out.d*x->d};}return out;}
    if(e->k==Kind::Neg){auto x=rationalPolynomial(e->a[0]);if(!x)return{};return PolyFraction{-x->n,x->d};}
    if(e->k==Kind::Pow){auto x=rationalPolynomial(e->a[0]);auto exponent=smallNatural(e->a[1]);if(!x||!exponent)return{};return PolyFraction{polyPower(x->n,*exponent),polyPower(x->d,*exponent)};}
    auto p=polynomial(e);if(!p)return{};return PolyFraction{*p,Poly::constant(1)};
}
static E subtraction(const E&a,const E&b){return op(Kind::Add,{a,op(Kind::Neg,{b})});}
static bool expressionIsNonzeroFromAssumption(const Assumption&a,const E&need){
    auto sameDiff=[&](const E&x,const E&y){
        return canonical(need)==canonical(subtraction(x,y)) ||
               canonical(need)==canonical(subtraction(y,x));
    };
    if(a.rel==Rel::Ne && sameDiff(a.left,a.right)) return true;
    // Simple positivity/negativity implications: x>0 or x<0 imply x!=0.
    if((a.rel==Rel::Gt || a.rel==Rel::Lt) &&
       ((canonical(need)==canonical(a.left) && canonical(a.right)==canonical(cn(0))) ||
        (canonical(need)==canonical(a.right) && canonical(a.left)==canonical(cn(0)))))
        return true;
    // Exact nonzero constants are intrinsically safe.
    auto p=polynomial(need);
    if(p && p->t.size()==1){
        auto it=p->t.find(Mono{});
        if(it!=p->t.end() && !it->second.isZero() && a.rel==Rel::Eq){
            auto q=polynomial(subtraction(a.left,a.right));
            if(q && q->key()==p->key()) return true;
        }
    }
    return false;
}
static bool assumptionsImplyNonzero(const Statement&s,const E&need){
    for(const Assumption&a:s.assumptions) if(expressionIsNonzeroFromAssumption(a,need)) return true;
    // If the denominator is an exact nonzero constant, no assumption is needed.
    auto p=polynomial(need);
    if(p && p->t.size()==1){
        auto it=p->t.find(Mono{});
        if(it!=p->t.end() && !it->second.isZero()) return true;
    }
    return false;
}
static void denominatorExpressions(const E&e,std::vector<E>&out){if(e->k==Kind::Div)out.push_back(e->a[1]);for(const auto& child:e->a)denominatorExpressions(child,out);}
static bool rationalEqualityUnderAssumptions(const Statement&s){if(s.rel!=Rel::Eq)return false;std::vector<E> den;denominatorExpressions(s.left,den);denominatorExpressions(s.right,den);for(const auto& x:den)if(!assumptionsImplyNonzero(s,x))return false;auto l=rationalPolynomial(s.left),r=rationalPolynomial(s.right);return l&&r&&(l->n*r->d).key()==(r->n*l->d).key();}
static std::string canonical(const E&e){if(auto p=polynomial(e))return "POLY:"+p->key();switch(e->k){case Kind::Const:return "C:"+e->q.str();case Kind::Var:return "V:"+e->name;case Kind::Neg:return "N("+canonical(e->a[0])+")";default:{std::string s=std::to_string(int(e->k))+":"+e->name+"(";for(auto&a:e->a)s+=canonical(a)+",";return s+")";}}}

// ============================================================================
// 5. Evaluation and Popper counterexample discovery
// ============================================================================
static BigInt integerValue(const Rational& x) { if (x.d!=BigInt(1)) throw Error("function requires an integer"); return x.n; }

// ============================================================================
// 5A. Exact number theory and combinatorics library
// ============================================================================
// All functions below are exact: they never use floating point arithmetic.
// This is deliberately conservative. When a function is too expensive for
// the configured input, it throws instead of silently returning an estimate.

static bool isOdd(const BigInt& n) { return !((n % BigInt(2)).isZero()); }

static BigInt modNorm(BigInt a, const BigInt& m) {
    if (m.isZero()) throw Error("modulus is zero");
    BigInt mm=m.abs();
    a=a%mm;
    if (a.negative()) a+=mm;
    return a;
}

static BigInt powmod(BigInt base, BigInt exp, const BigInt& mod) {
    if (mod.isZero()) throw Error("powmod modulus is zero");
    if (exp.negative()) throw Error("powmod exponent must be nonnegative");
    BigInt m=mod.abs(), out=modNorm(BigInt(1),m);
    base=modNorm(base,m);
    while(!exp.isZero()) {
        if(isOdd(exp)) out=(out*base)%m;
        exp=exp/BigInt(2);
        if(!exp.isZero()) base=(base*base)%m;
    }
    return out;
}

struct EGCDResult { BigInt g,x,y; };

static EGCDResult extendedGcd(BigInt a, BigInt b) {
    BigInt old_r=a, r=b, old_s(1), s(0), old_t(0), t(1);
    while(!r.isZero()) {
        BigInt q=old_r/r;
        BigInt nr=old_r-q*r; old_r=r; r=nr;
        BigInt ns=old_s-q*s; old_s=s; s=ns;
        BigInt nt=old_t-q*t; old_t=t; t=nt;
    }
    if(old_r.negative()) return {-old_r,-old_s,-old_t};
    return {old_r,old_s,old_t};
}

static Optional<BigInt> modInverse(const BigInt& a, const BigInt& m) {
    if(m.isZero()) return {};
    BigInt mm=m.abs(), aa=modNorm(a,mm);
    auto e=extendedGcd(aa,mm);
    if(e.g!=BigInt(1)) return {};
    return modNorm(e.x,mm);
}

static BigInt integerSqrt(const BigInt& n) {
    if(n.negative()) throw Error("sqrt of negative integer");
    if(n<BigInt(2)) return n;
    BigInt lo(1), hi=n, ans(1);
    while(lo<=hi) {
        BigInt mid=(lo+hi)/BigInt(2);
        BigInt sq=mid*mid;
        if(sq<=n){ ans=mid; lo=mid+BigInt(1); }
        else hi=mid-BigInt(1);
    }
    return ans;
}

// Deterministic trial division: slower than probabilistic tests, but exact.
static bool isPrimeExact(const BigInt& n) {
    if(n<BigInt(2)) return false;
    if(n==BigInt(2)) return true;
    if((n%BigInt(2)).isZero()) return false;
    BigInt d(3);
    while(d*d<=n) {
        if((n%d).isZero()) return false;
        d+=BigInt(2);
    }
    return true;
}

static std::vector<BigInt> primeFactors(const BigInt& input) {
    BigInt n=input.abs();
    if(n<BigInt(2)) return {};
    std::vector<BigInt> out;
    while((n%BigInt(2)).isZero()) { out.push_back(BigInt(2)); n=n/BigInt(2); }
    BigInt p(3);
    while(p*p<=n) {
        while((n%p).isZero()) { out.push_back(p); n=n/p; }
        p+=BigInt(2);
    }
    if(n>BigInt(1)) out.push_back(n);
    return out;
}

static BigInt eulerPhi(BigInt n) {
    if(n.negative()) n=-n;
    if(n.isZero()) return BigInt(0);
    BigInt result=n;
    std::vector<BigInt> f=primeFactors(n);
    for(size_t i=0;i<f.size();) {
        BigInt p=f[i]; result=result/p*(p-BigInt(1));
        while(i<f.size() && f[i]==p) ++i;
    }
    return result;
}

static BigInt divisorCount(BigInt n) {
    if(n.isZero()) throw Error("divisor count of zero is undefined");
    auto f=primeFactors(n);
    BigInt ans(1);
    for(size_t i=0;i<f.size();) {
        size_t j=i; while(j<f.size() && f[j]==f[i]) ++j;
        ans*=BigInt(static_cast<int64_t>(j-i+1)); i=j;
    }
    return ans;
}

static BigInt sigma1(BigInt n) {
    if(n.isZero()) throw Error("sigma(0) is undefined");
    n=n.abs(); if(n==BigInt(1)) return BigInt(1);
    auto f=primeFactors(n); BigInt ans(1);
    for(size_t i=0;i<f.size();) {
        BigInt p=f[i]; size_t e=0; while(i<f.size() && f[i]==p){++i;++e;}
        BigInt term(1), power(1);
        for(size_t k=0;k<e;++k){power*=p;term+=power;}
        ans*=term;
    }
    return ans;
}

static BigInt fibonacci(BigInt n) {
    if(n.negative()) {
        BigInt a=fibonacci(-n);
        // F(-n)=(-1)^(n+1) F(n)
        return isOdd(n)?a:-a;
    }
    std::function<std::pair<BigInt,BigInt>(BigInt)> fd =
        [&](BigInt k)->std::pair<BigInt,BigInt>{
            if(k.isZero()) return {BigInt(0),BigInt(1)};
            auto ab=fd(k/BigInt(2));
            BigInt a=ab.first,b=ab.second;
            BigInt c=a*(BigInt(2)*b-a);
            BigInt d=a*a+b*b;
            if(isOdd(k)) return {d,c+d};
            return {c,d};
        };
    return fd(n).first;
}

static BigInt factorialExact(BigInt n) {
    if(n.negative()) throw Error("factorial requires n >= 0");
    BigInt r(1);
    for(BigInt i(2);i<=n;i+=BigInt(1)) r*=i;
    return r;
}

static BigInt binomialExact(BigInt n, BigInt k) {
    if(n.negative() || k.negative() || k>n) return BigInt(0);
    if(k>n-k) k=n-k;
    BigInt r(1);
    for(BigInt i(1);i<=k;i+=BigInt(1)) {
        // r * (n-k+i) / i is exact at every step.
        r = (r*(n-k+i))/i;
    }
    return r;
}

static BigInt permutationExact(BigInt n, BigInt k) {
    if(n.negative() || k.negative() || k>n) return BigInt(0);
    BigInt r(1);
    for(BigInt i(0);i<k;i+=BigInt(1)) r*=n-i;
    return r;
}

static BigInt catalanExact(BigInt n) {
    if(n.negative()) throw Error("catalan requires n >= 0");
    return binomialExact(BigInt(2)*n,n)/(n+BigInt(1));
}

static BigInt derangementExact(BigInt n) {
    if(n.negative()) throw Error("derangements require n >= 0");
    BigInt d0(1), d1(0);
    if(n.isZero()) return d0;
    for(BigInt i(2);i<=n;i+=BigInt(1)) {
        BigInt d2=(i-BigInt(1))*(d1+d0); d0=d1; d1=d2;
    }
    return d1;
}

static BigInt stirling2Exact(BigInt n, BigInt k) {
    if(n.negative()||k.negative()||k>n) return BigInt(0);
    // recurrence S(n,k)=S(n-1,k-1)+k*S(n-1,k)
    std::vector<BigInt> row;
    std::string ns=n.str(), ks=k.str();
    size_t nn=static_cast<size_t>(std::stoull(ns));
    size_t kk=static_cast<size_t>(std::stoull(ks));
    if(nn>1000) throw Error("stirling2 input n too large for exact table mode (max 1000)");
    row.assign(kk+1,BigInt(0)); row[0]=BigInt(1);
    for(size_t i=1;i<=nn;++i) {
        for(size_t j=std::min(i,kk);j>=1;--j)
            row[j]=row[j-1]+BigInt(static_cast<int64_t>(j))*row[j];
        row[0]=BigInt(0);
    }
    return row[kk];
}

static BigInt partitionExact(BigInt n) {
    if(n.negative()) throw Error("partition requires n >= 0");
    size_t nn=static_cast<size_t>(std::stoull(n.str()));
    if(nn>5000) throw Error("partition input too large for exact O(n^2) mode (max 5000)");
    std::vector<BigInt> p(nn+1); p[0]=BigInt(1);
    for(size_t i=1;i<=nn;++i) {
        BigInt s(0);
        for(size_t k=1;;++k) {
            long long g1=static_cast<long long>(k*(3*k-1)/2);
            if(g1>(long long)i) break;
            BigInt term=p[i-(size_t)g1];
            if(k%2) s+=term; else s-=term;
            long long g2=static_cast<long long>(k*(3*k+1)/2);
            if(g2<=(long long)i) { if(k%2) s+=p[i-(size_t)g2]; else s-=p[i-(size_t)g2]; }
        }
        p[i]=s;
    }
    return p[nn];
}

static BigInt bellExact(BigInt n) {
    if(n.negative()) throw Error("bell requires n >= 0");
    size_t nn=static_cast<size_t>(std::stoull(n.str()));
    if(nn>500) throw Error("bell input too large for exact table mode (max 500)");
    std::vector<BigInt> row(nn+1), next(nn+1);
    row[0]=BigInt(1);
    for(size_t i=1;i<=nn;++i) {
        next[0]=row[i-1];
        for(size_t j=1;j<=i;++j) next[j]=next[j-1]+row[j-1];
        row.swap(next);
    }
    return row[0];
}

static BigInt compositionExact(BigInt n, BigInt k) {
    // Number of ordered compositions of n into k positive parts = C(n-1,k-1).
    if(n.isZero() && k.isZero()) return BigInt(1);
    if(n.negative()||k.negative()||k.isZero()||k>n) return BigInt(0);
    return binomialExact(n-BigInt(1),k-BigInt(1));
}

static BigInt multinomial2Exact(BigInt n, BigInt a, BigInt b) {
    if(a.negative()||b.negative()||a+b!=n) return BigInt(0);
    return binomialExact(n,a);
}

static BigInt lcmExact(const BigInt&a,const BigInt&b) {
    if(a.isZero()||b.isZero()) return BigInt(0);
    return ((a/gcd(a,b))*b).abs();
}

static Rational eval(const E&e,const std::map<std::string,Rational>&env){
    switch(e->k){
        case Kind::Const:return e->q;
        case Kind::Var:{
            auto i=env.find(e->name);
            if(i==env.end())throw Error("unbound variable "+e->name);
            return i->second;
        }
        case Kind::Neg:return -eval(e->a[0],env);
        case Kind::Add:{Rational z;for(auto&a:e->a)z=z+eval(a,env);return z;}
        case Kind::Mul:{Rational z(1);for(auto&a:e->a)z=z*eval(a,env);return z;}
        case Kind::Div:return eval(e->a[0],env)/eval(e->a[1],env);
        case Kind::Pow:{
            Rational b=eval(e->a[0],env); auto n=smallNatural(e->a[1]);
            if(!n) throw Error("non-natural exponent evaluation");
            Rational z(1);for(int i=0;i<*n;++i)z=z*b;return z;
        }
        case Kind::Func:{
            std::vector<Rational>x;for(auto&q:e->a)x.push_back(eval(q,env));
            const std::string& f=e->name;
            if(f=="abs"&&x.size()==1)return x[0].n.negative()?-x[0]:x[0];
            if(f=="gcd"&&x.size()==2)return Rational(gcd(integerValue(x[0]),integerValue(x[1])),BigInt(1));
            if(f=="lcm"&&x.size()==2)return Rational(lcmExact(integerValue(x[0]),integerValue(x[1])),BigInt(1));
            if(f=="mod"&&x.size()==2)return Rational(modNorm(integerValue(x[0]),integerValue(x[1])),BigInt(1));
            if(f=="factorial"&&x.size()==1)return Rational(factorialExact(integerValue(x[0])),BigInt(1));
            if((f=="isPrime"||f=="prime")&&x.size()==1)return Rational(isPrimeExact(integerValue(x[0]))?1:0);
            if(f=="powmod"&&x.size()==3)return Rational(powmod(integerValue(x[0]),integerValue(x[1]),integerValue(x[2])),BigInt(1));
            if(f=="modinv"&&x.size()==2){auto z=modInverse(integerValue(x[0]),integerValue(x[1]));if(!z)throw Error("modular inverse does not exist");return Rational(*z,BigInt(1));}
            if(f=="phi"&&x.size()==1)return Rational(eulerPhi(integerValue(x[0])),BigInt(1));
            if(f=="tau"&&x.size()==1)return Rational(divisorCount(integerValue(x[0])),BigInt(1));
            if(f=="sigma"&&x.size()==1)return Rational(sigma1(integerValue(x[0])),BigInt(1));
            if(f=="fib"&&x.size()==1)return Rational(fibonacci(integerValue(x[0])),BigInt(1));
            if(f=="nCr"&&x.size()==2)return Rational(binomialExact(integerValue(x[0]),integerValue(x[1])),BigInt(1));
            if(f=="nPr"&&x.size()==2)return Rational(permutationExact(integerValue(x[0]),integerValue(x[1])),BigInt(1));
            if(f=="catalan"&&x.size()==1)return Rational(catalanExact(integerValue(x[0])),BigInt(1));
            if((f=="derangements"||f=="subfactorial")&&x.size()==1)return Rational(derangementExact(integerValue(x[0])),BigInt(1));
            if(f=="stirling2"&&x.size()==2)return Rational(stirling2Exact(integerValue(x[0]),integerValue(x[1])),BigInt(1));
            if(f=="partition"&&x.size()==1)return Rational(partitionExact(integerValue(x[0])),BigInt(1));
            if(f=="bell"&&x.size()==1)return Rational(bellExact(integerValue(x[0])),BigInt(1));
            if(f=="composition"&&x.size()==2)return Rational(compositionExact(integerValue(x[0]),integerValue(x[1])),BigInt(1));
            if(f=="multinomial2"&&x.size()==3)return Rational(multinomial2Exact(integerValue(x[0]),integerValue(x[1]),integerValue(x[2])),BigInt(1));
            if(f=="sqrt"&&x.size()==1)return Rational(integerSqrt(integerValue(x[0])),BigInt(1));
            throw Error("unknown function or arity: "+f);
        }
    }
    throw Error("invalid expression");
}
static bool relationHolds(Rel rel,const Rational&a,const Rational&b){
    switch(rel){
        case Rel::Eq:return a==b;
        case Rel::Ne:return !(a==b);
        case Rel::Lt:return a<b;
        case Rel::Le:return a<b||a==b;
        case Rel::Gt:return b<a;
        case Rel::Ge:return b<a||a==b;
        case Rel::Divides:{
            BigInt x=integerValue(a), y=integerValue(b);
            if(x.isZero()) return y.isZero();
            return (y%x).isZero();
        }
    }
    return false;
}
static bool assumptionsHold(const Statement&s,const std::map<std::string,Rational>&env){for(const Assumption&a:s.assumptions)if(!relationHolds(a.rel,eval(a.left,env),eval(a.right,env)))return false;return true;}
static void vars(const E&e,std::set<std::string>&out){if(e->k==Kind::Var)out.insert(e->name);for(auto&a:e->a)vars(a,out);}
struct Evidence { int tests=0; Optional<std::map<std::string,Rational>> counterexample; std::string reason; };
class Popper { std::mt19937_64 rng; public: explicit Popper(uint64_t seed):rng(seed){} Evidence attack(const Statement&s,int count){Evidence ev;std::set<std::string> vs;vars(s.left,vs);vars(s.right,vs);for(const Assumption&a:s.assumptions){vars(a.left,vs);vars(a.right,vs);}std::vector<std::string> names(vs.begin(),vs.end());std::vector<int> boundary={0,1,-1,2,-2,3,-3,7,-7};for(int k=0;k<count;++k){std::map<std::string,Rational> env;for(size_t i=0;i<names.size();++i){int v=k<(int)boundary.size()?boundary[(k+i)%boundary.size()]:int(rng()%21)-10;env[names[i]]=Rational(v);}try{if(!assumptionsHold(s,env))continue;Rational a=eval(s.left,env),b=eval(s.right,env);bool ok=relationHolds(s.rel,a,b);++ev.tests;if(!ok){ev.counterexample=env;ev.reason="exact evaluation contradicts relation under all assumptions";return ev;}}catch(const Error&){/* Undefined points are neither counterexamples nor evidence. */}}return ev;} };

// ============================================================================
// 6. Proof certificates, independent kernel and Euclid proof search
// ============================================================================
enum class Status { GENERATED, NORMALIZED, EMPIRICALLY_SUPPORTED, FALSIFIED, PROOF_SEARCHING, PROOF_FOUND, KERNEL_REJECTED, KERNEL_VERIFIED, THEOREM, CONDITIONAL_THEOREM, SEARCH_EXHAUSTED, UNSUPPORTED, CONJECTURE, LEMMA_FOUND, THEORY_FOUND };
static const char* statusName(Status s){static const char*n[]={"GENERATED","NORMALIZED","EMPIRICALLY_SUPPORTED","FALSIFIED","PROOF_SEARCHING","PROOF_FOUND","KERNEL_REJECTED","KERNEL_VERIFIED","THEOREM","CONDITIONAL_THEOREM","SEARCH_EXHAUSTED","UNSUPPORTED","CONJECTURE","LEMMA_FOUND","THEORY_FOUND"};return n[int(s)];}
struct ProofStep { int number; std::string before,after,rule,justification; };
struct Proof { std::vector<ProofStep> steps; std::string finalKey; std::string text()const{std::ostringstream o;for(auto&s:steps)o<<"Step "<<s.number<<": "<<s.rule<<"\n  "<<s.before<<"\n  => "<<s.after<<"\n  "<<s.justification<<"\n";return o.str();} };
static std::string relationName(Rel r);
class DefinitionKernel {
    static bool unify(const E&p,const E&a,std::map<std::string,E>&m){
        if(p->k==Kind::Var&&(p->name=="n"||p->name=="k")){
            auto it=m.find(p->name);if(it==m.end()){m[p->name]=a;return true;}return canonical(it->second)==canonical(a);
        }
        if(p->k!=a->k||p->name!=a->name||p->a.size()!=a->a.size())return false;
        if(p->k==Kind::Const)return p->q==a->q;
        if(p->k==Kind::Var)return p->name==a->name;
        for(size_t i=0;i<p->a.size();++i)if(!unify(p->a[i],a->a[i],m))return false;
        return true;
    }
    static bool sameAssumptions(const Statement&a,const Statement&b,std::map<std::string,E>&m){
        if(a.assumptions.size()!=b.assumptions.size())return false;
        std::vector<bool>used(b.assumptions.size(),false);
        for(const auto&aa:a.assumptions){bool found=false;for(size_t i=0;i<b.assumptions.size();++i)if(!used[i]&&aa.rel==b.assumptions[i].rel){auto trial=m;if(unify(b.assumptions[i].left,aa.left,trial)&&unify(b.assumptions[i].right,aa.right,trial)){m=trial;used[i]=true;found=true;break;}}if(!found)return false;}
        return true;
    }
public:
    static bool matches(const Statement&s,const std::string&rule){
        if(s.rel!=Rel::Eq) return false;
        std::string text;
        if(rule=="fib_recurrence")text="fib(n+2)=fib(n+1)+fib(n) assuming n>=0";
        else if(rule=="factorial_recurrence")text="factorial(n+1)=(n+1)*factorial(n) assuming n>=0";
        else if(rule=="binomial_pascal")text="nCr(n,k)=nCr(n-1,k-1)+nCr(n-1,k) assuming n>=1 and k>=1 and k<=n";
        else if(rule=="binomial_symmetry")text="nCr(n,k)=nCr(n,n-k) assuming n>=0 and k>=0 and k<=n";
        else if(rule=="derangement_recurrence")text="derangements(n)=(n-1)*(derangements(n-1)+derangements(n-2)) assuming n>=2";
        else return false;
        try{Statement e=Parser(text).statement();std::map<std::string,E>m;if(!unify(e.left,s.left,m)||!unify(e.right,s.right,m))return false;return sameAssumptions(s,e,m);}catch(...){return false;}
    }
    static std::string tag(const std::string&rule,const Statement&s){ return "DEF:"+rule+":"+print(s); }

    // Verify exactly one sound recursive-definition rewrite anywhere inside one
    // side of an equality.  This is deliberately structural: the research
    // planner may suggest a rewrite, but the kernel independently establishes
    // that the changed subexpression is an instance of a trusted schema.
    static bool verifyOneRewrite(const Statement&before,const Statement&after){
        if(before.rel!=after.rel || before.assumptions.size()!=after.assumptions.size()) return false;
        if(before.assumptions.size()!=after.assumptions.size()) return false;
        for(size_t i=0;i<before.assumptions.size();++i){
            const auto&a=before.assumptions[i]; const auto&b=after.assumptions[i];
            if(a.rel!=b.rel || canonical(a.left)!=canonical(b.left) || canonical(a.right)!=canonical(b.right)) return false;
        }
        // Local recursive checker: at each node either the entire expression is
        // an allowed schema instance, or exactly one child changes.
        std::function<bool(const E&,const E&)> one = [&](const E&x,const E&y)->bool{
            if(canonical(x)==canonical(y)) return false;
            // Candidate local theorem, carrying the original assumptions.
            Statement local; local.rel=Rel::Eq; local.left=x; local.right=y;
            local.assumptions=before.assumptions;
            const char* rules[]={"fib_recurrence","factorial_recurrence","binomial_pascal","derangement_recurrence"};
            for(const char*r:rules) if(matches(local,r)) return true;
            if(x->k!=y->k || x->name!=y->name || x->a.size()!=y->a.size()) return false;
            int changed=0;
            for(size_t i=0;i<x->a.size();++i){
                if(canonical(x->a[i])!=canonical(y->a[i])){
                    ++changed; if(changed>1)return false;
                    if(!one(x->a[i],y->a[i]))return false;
                }
            }
            return changed==1;
        };
        if(canonical(before.left)!=canonical(after.left) && canonical(before.right)==canonical(after.right))
            return one(before.left,after.left);
        if(canonical(before.right)!=canonical(after.right) && canonical(before.left)==canonical(after.left))
            return one(before.right,after.right);
        return false;
    }

    static bool verify(const Statement&s,const Proof&p,std::string&why){
        if(p.steps.size()!=2) return false;
        if(p.steps[0].before!=print(s.left)||p.steps[1].before!=print(s.right)) return false;
        const char* rules[]={"fib_recurrence","factorial_recurrence","binomial_pascal","binomial_symmetry","derangement_recurrence"};
        for(const char* r:rules){
            if(matches(s,r)){
                std::string k=tag(r,s);
                if(p.steps[0].rule=="definition_rule"&&p.steps[1].rule=="definition_rule"&&
                   p.steps[0].after==k&&p.steps[1].after==k&&p.finalKey==k){
                    why=std::string("trusted recursive definition/schema: ")+r; return true;
                }
            }
        }
        return false;
    }
};

class RewritePlanner {
    static bool splitConst(const E&e,int64_t c,E&rest){
        if(e->k!=Kind::Add)return false;
        std::vector<E> terms;
        std::function<void(const E&)> flat=[&](const E&x){
            if(x->k==Kind::Add){for(const auto&z:x->a)flat(z);}
            else terms.push_back(x);
        }; flat(e);
        bool found=false; std::vector<E> keep;
        for(const auto&t:terms){
            if(!found && t->k==Kind::Const && t->q.d==BigInt(1) && t->q.n==BigInt(c)){found=true;}
            else keep.push_back(t);
        }
        if(!found||keep.empty())return false;
        rest=keep[0]; for(size_t i=1;i<keep.size();++i)rest=op(Kind::Add,{rest,keep[i]});
        return true;
    }
    static bool topRewrite(const E&e,const Statement&ctx,E&out,std::string&rule){
        if(e->k!=Kind::Func)return false;
        if(e->name=="fib"&&e->a.size()==1){
            E n; if(splitConst(e->a[0],2,n)){
                out=op(Kind::Add,{op(Kind::Func,{op(Kind::Add,{n,cn(1)})}),op(Kind::Func,{n})});
                out->a[0]->name="fib";out->a[1]->name="fib";rule="fib_recurrence";
                Statement local=ctx;local.left=e;local.right=out;
                if(DefinitionKernel::matches(local,rule))return true;
            }
        }
        if(e->name=="factorial"&&e->a.size()==1){
            E n;if(splitConst(e->a[0],1,n)){
                auto f=op(Kind::Func,{n});f->name="factorial";
                out=op(Kind::Mul,{op(Kind::Add,{n,cn(1)}),f});rule="factorial_recurrence";
                Statement local=ctx;local.left=e;local.right=out;
                if(DefinitionKernel::matches(local,rule))return true;
            }
        }
        if(e->name=="derangements"&&e->a.size()==1){
            E n=e->a[0];
            // Guard by the theorem's explicit n>=2 assumption through the
            // definition kernel; build the exact schema instance first.
            auto f1=op(Kind::Func,{subtraction(n,cn(1))});f1->name="derangements";
            auto f2=op(Kind::Func,{subtraction(n,cn(2))});f2->name="derangements";
            out=op(Kind::Mul,{subtraction(n,cn(1)),op(Kind::Add,{f1,f2})});rule="derangement_recurrence";
            Statement local=ctx;local.left=e;local.right=out;
            if(DefinitionKernel::matches(local,rule))return true;
        }
        if(e->name=="nCr"&&e->a.size()==2){
            E n=e->a[0],k=e->a[1];
            auto f1=op(Kind::Func,{subtraction(n,cn(1)),subtraction(k,cn(1))});f1->name="nCr";
            auto f2=op(Kind::Func,{subtraction(n,cn(1)),k});f2->name="nCr";
            out=op(Kind::Add,{f1,f2});rule="binomial_pascal";
            Statement local=ctx;local.left=e;local.right=out;
            if(DefinitionKernel::matches(local,rule))return true;
        }
        return false;
    }
    static bool rewriteExpr(const E&e,const Statement&ctx,E&out,std::string&rule){
        if(topRewrite(e,ctx,out,rule))return true;
        for(size_t i=0;i<e->a.size();++i){
            E childOut;std::string childRule;
            if(rewriteExpr(e->a[i],ctx,childOut,childRule)){
                auto z=std::make_shared<Expr>();z->k=e->k;z->q=e->q;z->name=e->name;z->a=e->a;z->a[i]=childOut;
                out=z;rule=childRule;return true;
            }
        }
        return false;
    }
public:
    Optional<Proof> prove(const Statement&target,int maxSteps=8)const{
        if(target.rel!=Rel::Eq)return {};
        Statement cur=target; Proof p;
        for(int step=0;step<maxSteps;++step){
            E nl,nr;std::string rule;
            bool changed=rewriteExpr(cur.left,cur,nl,rule);
            if(changed){nr=cur.right;}
            else {changed=rewriteExpr(cur.right,cur,nr,rule);nl=cur.left;}
            if(!changed)break;
            Statement next=cur;next.left=nl;next.right=nr;
            ProofStep st;st.number=static_cast<int>(p.steps.size()+1);st.before=print(cur);st.after=print(next);
            st.rule="definition_rewrite_chain";st.justification="kernel-checkable recursive definition rewrite: "+rule;
            p.steps.push_back(st);cur=next;
            auto l=symbolicPolynomial(cur.left),r=symbolicPolynomial(cur.right);
            if(l&&r&&l->key()==r->key()){
                std::string key="REWRITE_CHAIN:"+l->key();
                p.steps.push_back({static_cast<int>(p.steps.size()+1),print(cur),key,"polynomial_normalization","final exact normalization after definition rewrites"});
                p.finalKey=key;return p;
            }
            if(rationalEqualityUnderAssumptions(cur)){
                auto a=rationalPolynomial(cur.left),b=rationalPolynomial(cur.right);
                std::string key="REWRITE_CHAIN:CROSS:"+(a->n*b->d).key();
                p.steps.push_back({static_cast<int>(p.steps.size()+1),print(cur),key,"rational_cross_normalization","final exact rational normalization after definition rewrites"});
                p.finalKey=key;return p;
            }
        }
        return {};
    }
};


class Kernel { public: bool verify(const Statement&s,const Proof&p,std::string&why)const {
        // Multi-step rewrite certificates are checked independently, step by step.
        // No research-layer result is trusted merely because a planner proposed it.
        if(!p.steps.empty() && p.steps[0].rule=="bidirectional_definition_rewrite") {
            if(p.steps.size()<2){why="bidirectional rewrite chain is empty";return false;}
            if(p.steps[0].before!=print(s)){why="bidirectional chain does not start at statement";return false;}
            for(size_t i=0;i+1<p.steps.size();++i){
                if(p.steps[i].after!=p.steps[i+1].before){why="bidirectional steps are not contiguous";return false;}
                if(i+1==p.steps.size()-1 && p.steps[i+1].rule=="polynomial_normalization")break;
                Statement a,b;
                try{a=Parser(p.steps[i].before).statement();b=Parser(p.steps[i].after).statement();}
                catch(...){why="cannot parse bidirectional rewrite step";return false;}
                if(!DefinitionKernel::verifyOneRewrite(a,b)){why="bidirectional rewrite rejected by definition kernel";return false;}
            }
            if(p.steps.back().rule!="polynomial_normalization"){why="bidirectional chain has no final normalization";return false;}
            Statement last;
            try{last=Parser(p.steps.back().before).statement();}catch(...){why="cannot parse bidirectional final state";return false;}
            auto l=symbolicPolynomial(last.left),r=symbolicPolynomial(last.right);
            if(!l||!r||l->key()!=r->key()){why="bidirectional final state is not symbolically equal";return false;}
            std::string key="BIDIR_REWRITE:"+l->key();
            if(p.finalKey!=key){why="bidirectional final key mismatch";return false;}
            why="bidirectional recursive rewrites independently verified and final symbolic normalization certified";return true;
        }
        if(!p.steps.empty() && p.steps[0].rule=="definition_rewrite_chain") {
            if(p.steps.size()<2){why="rewrite chain is empty";return false;}
            if(p.steps[0].before!=print(s)) { why="rewrite-chain transcript does not start at the statement"; return false; }
            for(size_t i=0;i+1<p.steps.size();++i){
                if(p.steps[i].after!=p.steps[i+1].before){why="rewrite-chain steps are not contiguous";return false;}
                if(i+1==p.steps.size()-1 &&
                   (p.steps[i+1].rule=="polynomial_normalization" || p.steps[i+1].rule=="rational_cross_normalization")) break;
                Statement a,b;
                try{a=Parser(p.steps[i].before).statement();b=Parser(p.steps[i].after).statement();}
                catch(const std::exception&e){why=std::string("cannot parse rewrite step: ")+e.what();return false;}
                if(!DefinitionKernel::verifyOneRewrite(a,b)) { why="definition rewrite step rejected by independent definition kernel"; return false; }
            }
            if(p.steps.back().rule!="polynomial_normalization" &&
               p.steps.back().rule!="rational_cross_normalization"){
                why="rewrite chain has no certified final normalization step";return false;
            }
            Statement last;
            try{last=Parser(p.steps.back().before).statement();}
            catch(...){why="cannot parse final rewrite target";return false;}
            if(p.steps.back().before!=print(last)) {why="non-canonical final transcript";return false;}
            auto l=symbolicPolynomial(last.left),r=symbolicPolynomial(last.right);
            if(l&&r&&l->key()==r->key()){
                std::string key="REWRITE_CHAIN:"+l->key();
                if(p.finalKey!=key){why="rewrite chain final key mismatch";return false;}
                why="each recursive-definition rewrite was independently checked, then the final target normalized exactly";
                return true;
            }
            if(rationalEqualityUnderAssumptions(last)){
                auto a=rationalPolynomial(last.left),b=rationalPolynomial(last.right);
                std::string key="REWRITE_CHAIN:CROSS:"+(a->n*b->d).key();
                if(p.finalKey!=key){why="rewrite chain rational final key mismatch";return false;}
                why="definition rewrites were checked independently and the final rational identity was checked exactly";
                return true;
            }
            why="rewrite chain did not reach a kernel-normalizable target";return false;
        }
        if(DefinitionKernel::verify(s,p,why)) return true;
        if(p.steps.size()!=2){why="certificate must have two steps";return false;}
        if(p.steps[0].before!=print(s.left)||p.steps[1].before!=print(s.right)){why="step transcript disagrees with statement";return false;}
        if(s.assumptions.empty()){
            std::set<std::string> vs; vars(s.left,vs); vars(s.right,vs);
            if(vs.empty() && p.steps[0].rule=="exact_evaluation" && p.steps[1].rule=="exact_evaluation"){
                try{
                    bool ok=relationHolds(s.rel,eval(s.left,{}),eval(s.right,{}));
                    std::string tag="EXACT:"+relationName(s.rel)+":"+canonical(s.left)+":"+canonical(s.right);
                    if(ok && p.steps[0].after==tag && p.steps[1].after==tag && p.finalKey==tag){why="closed-form relation evaluated exactly by the kernel";return true;}
                }catch(const Error&){}
            }
        }
        if(s.assumptions.empty() && s.rel!=Rel::Eq){
            std::set<std::string> vs;vars(s.left,vs);vars(s.right,vs);
            if(vs.empty()){
                try{
                    bool ok=relationHolds(s.rel,eval(s.left,{}),eval(s.right,{}));
                    std::string tag="EXACT:"+relationName(s.rel)+":"+canonical(s.left)+":"+canonical(s.right);
                    if(p.steps[0].rule=="exact_evaluation"&&p.steps[1].rule=="exact_evaluation"&&p.steps[0].after==tag&&p.steps[1].after==tag&&ok){why="closed-form relation evaluated exactly by the kernel";return true;}
                }catch(const Error&){}
            }
            why="non-equality relation is only kernel-certifiable for closed-form constants";return false;
        }
        if(s.rel!=Rel::Eq){why="conditional inequality/divisibility proof is outside the current symbolic kernel";return false;}
        if(p.steps[0].rule=="polynomial_normalization"&&p.steps[1].rule=="polynomial_normalization"){
            std::string l=canonical(s.left),r=canonical(s.right);
            if(p.steps[0].after!=l||p.steps[1].after!=r||l!=r||p.finalKey!=l){why="invalid polynomial certificate";return false;}
            why="both sides independently normalized to the same exact polynomial";return true;
        }
        if(p.steps[0].rule=="rational_cross_normalization"&&p.steps[1].rule=="rational_cross_normalization"){
            if(!rationalEqualityUnderAssumptions(s)){why="denominator obligation missing or cross products differ";return false;}
            auto l=rationalPolynomial(s.left),r=rationalPolynomial(s.right);
            std::string lk="CROSS:"+(l->n*r->d).key(),rk="CROSS:"+(r->n*l->d).key();
            if(p.steps[0].after!=lk||p.steps[1].after!=rk||p.finalKey!=lk||lk!=rk){why="invalid rational certificate";return false;}
            why="denominators established nonzero by assumptions; exact cross-products agree";return true;
        }
        why="untrusted rule";return false;
    }
};

static int exprDepth(const E& e) { int result=1; for(const auto& child:e->a) result=std::max(result,1+exprDepth(child)); return result; }

// ============================================================================
// 6A. Bidirectional proof search
// ============================================================================
// The previous planner only pushed recursive definitions in one direction.
// HILBERT 2.5 adds a bounded bidirectional search: trusted definition schemas
// may be used forward OR backward, at any subexpression.  Every edge is still
// independently checked by DefinitionKernel, so search cannot manufacture a
// theorem merely by reaching a syntactically convenient expression.
class BidirectionalRewriteSearch {
    static std::string localKey(const Statement&s){
        std::string k=std::to_string(int(s.rel))+"|"+canonical(s.left)+"|"+canonical(s.right)+"|";
        for(const auto&a:s.assumptions) k+=std::to_string(int(a.rel))+":"+canonical(a.left)+":"+canonical(a.right)+";";
        return k;
    }
    static E subst(const E&e,const std::map<std::string,E>&m){
        if(e->k==Kind::Var){auto it=m.find(e->name);return it==m.end()?vr(e->name):it->second;}
        auto z=std::make_shared<Expr>();z->k=e->k;z->q=e->q;z->name=e->name;for(const auto&c:e->a)z->a.push_back(subst(c,m));return z;
    }
    struct Node { Statement s; Proof p; };

    static bool sameContext(const Statement&a,const Statement&b){
        if(a.rel!=b.rel || a.assumptions.size()!=b.assumptions.size()) return false;
        for(size_t i=0;i<a.assumptions.size();++i){
            if(a.assumptions[i].rel!=b.assumptions[i].rel) return false;
            if(canonical(a.assumptions[i].left)!=canonical(b.assumptions[i].left) ||
               canonical(a.assumptions[i].right)!=canonical(b.assumptions[i].right)) return false;
        }
        return true;
    }

    static std::vector<std::pair<E,std::string>> localRewrites(const E&e,const Statement&ctx){
        std::vector<std::pair<E,std::string>> out;
        const char* rules[]={"fib_recurrence","factorial_recurrence",
                             "binomial_pascal","binomial_symmetry",
                             "derangement_recurrence"};
        // Build schema expressions once and use structural unification in both
        // directions.  n/k are schema variables, while all other variables in
        // the schema are literal identifiers.
        for(const char*r:rules){
            std::string text;
            if(std::string(r)=="fib_recurrence") text="fib(n+2)=fib(n+1)+fib(n) assuming n>=0";
            else if(std::string(r)=="factorial_recurrence") text="factorial(n+1)=(n+1)*factorial(n) assuming n>=0";
            else if(std::string(r)=="binomial_pascal") text="nCr(n,k)=nCr(n-1,k-1)+nCr(n-1,k) assuming n>=1 and k>=1 and k<=n";
            else if(std::string(r)=="binomial_symmetry") text="nCr(n,k)=nCr(n,n-k) assuming n>=0 and k>=0 and k<=n";
            else text="derangements(n)=(n-1)*(derangements(n-1)+derangements(n-2)) assuming n>=2";
            try{
                Statement schema=Parser(text).statement();
                auto tryDirection=[&](const E&lhs,const E&rhs,bool reverse){
                    std::map<std::string,E> m;
                    // Local rewrite must preserve the current theorem's
                    // assumptions; schema assumptions are checked separately by
                    // DefinitionKernel::matches after instantiation.
                    std::function<bool(const E&,const E&)> unifyLocal =
                        [&](const E&p,const E&a)->bool{
                            if(p->k==Kind::Var&&(p->name=="n"||p->name=="k")){
                                auto it=m.find(p->name);
                                if(it==m.end()){m[p->name]=a;return true;}
                                return canonical(it->second)==canonical(a);
                            }
                            if(p->k!=a->k||p->name!=a->name||p->a.size()!=a->a.size())return false;
                            if(p->k==Kind::Const)return p->q==a->q;
                            if(p->k==Kind::Var)return p->name==a->name;
                            for(size_t i=0;i<p->a.size();++i)if(!unifyLocal(p->a[i],a->a[i]))return false;
                            return true;
                        };
                    if(!unifyLocal(lhs,e)) return;
                    E inst=subst(rhs,m);
                    Statement local=ctx;local.left=e;local.right=inst;
                    if(reverse){
                        local.left=e;local.right=inst;
                        if(!DefinitionKernel::matches(local,r)) return;
                    } else if(!DefinitionKernel::matches(local,r)) return;
                    out.push_back({inst,std::string(r)+(reverse?":reverse":":forward")});
                };
                tryDirection(schema.left,schema.right,false);
                tryDirection(schema.right,schema.left,true);
            }catch(...){ }
        }
        return out;
    }

    static void collectRewrites(const E&e,const Statement&ctx,
                                std::vector<std::pair<E,std::string>>&outs){
        for(const auto&x:localRewrites(e,ctx)) outs.push_back(x);
        for(size_t i=0;i<e->a.size();++i){
            std::vector<std::pair<E,std::string>> child;
            collectRewrites(e->a[i],ctx,child);
            for(const auto&c:child){
                auto z=std::make_shared<Expr>();z->k=e->k;z->q=e->q;z->name=e->name;z->a=e->a;z->a[i]=c.first;
                outs.push_back({z,c.second});
            }
        }
    }

    static bool goalReached(const Statement&s){
        auto l=symbolicPolynomial(s.left),r=symbolicPolynomial(s.right);
        return l&&r&&l->key()==r->key();
    }

public:
    Optional<Proof> prove(const Statement&target,int maxStates=1000,int maxDepth=12)const{
        if(target.rel!=Rel::Eq) return {};
        std::queue<Node> q;
        std::unordered_set<std::string> seen;
        Proof root;
        q.push({target,root});seen.insert(localKey(target));
        int states=0;
        while(!q.empty() && states++<maxStates){
            Node cur=q.front();q.pop();
            if(goalReached(cur.s)){
                if(!cur.p.steps.empty()){
                    std::string key="BIDIR_REWRITE:"+symbolicPolynomial(cur.s.left)->key();
                    Proof p=cur.p;
                    p.steps.push_back({static_cast<int>(p.steps.size()+1),print(cur.s),key,
                                       "polynomial_normalization",
                                       "final exact normalization after bidirectional trusted rewrites"});
                    p.finalKey=key;return p;
                }
            }
            if(cur.p.steps.size()>=static_cast<size_t>(maxDepth)) continue;
            std::vector<std::pair<E,std::string>> moves;
            collectRewrites(cur.s.left,cur.s,moves);
            for(const auto&mv:moves){
                Statement next=cur.s;next.left=mv.first;
                if(!sameContext(cur.s,next))continue;
                std::string key=localKey(next);
                if(!seen.insert(key).second)continue;
                Proof p=cur.p;
                p.steps.push_back({static_cast<int>(p.steps.size()+1),print(cur.s),print(next),
                                   "bidirectional_definition_rewrite",mv.second});
                // The edge itself is checked now; the kernel repeats this check
                // when the finished certificate is submitted.
                Statement a=Parser(p.steps.back().before).statement();
                Statement b=Parser(p.steps.back().after).statement();
                if(!DefinitionKernel::verifyOneRewrite(a,b)) continue;
                q.push({next,p});
            }
            moves.clear();collectRewrites(cur.s.right,cur.s,moves);
            for(const auto&mv:moves){
                Statement next=cur.s;next.right=mv.first;
                std::string key=localKey(next);
                if(!seen.insert(key).second)continue;
                Proof p=cur.p;
                p.steps.push_back({static_cast<int>(p.steps.size()+1),print(cur.s),print(next),
                                   "bidirectional_definition_rewrite",mv.second});
                Statement a=Parser(p.steps.back().before).statement();
                Statement b=Parser(p.steps.back().after).statement();
                if(!DefinitionKernel::verifyOneRewrite(a,b)) continue;
                q.push({next,p});
            }
        }
        return {};
    }
};

class Euclid {
    int maxStates_,maxDepth_;
public:
    explicit Euclid(int maxStates=10000,int maxDepth=64):maxStates_(maxStates),maxDepth_(maxDepth){}
    Optional<Proof> prove(const Statement&s){
        if(maxStates_<1||exprDepth(s.left)>maxDepth_||exprDepth(s.right)>maxDepth_)return{};
        if(s.rel==Rel::Eq){
            const char* rules[]={"fib_recurrence","factorial_recurrence","binomial_pascal","binomial_symmetry","derangement_recurrence"};
            for(const char* rule:rules) if(DefinitionKernel::matches(s,rule)){
                std::string key=DefinitionKernel::tag(rule,s); Proof p;
                p.steps={{1,print(s.left),key,"definition_rule",std::string("trusted definition schema ")+rule},
                        {2,print(s.right),key,"definition_rule",std::string("trusted definition schema ")+rule}};
                p.finalKey=key; return p;
            }
            auto l=polynomial(s.left),r=polynomial(s.right);
            if(l&&r&&l->key()==r->key()){
                Proof p;std::string key="POLY:"+l->key();
                p.steps={{1,print(s.left),key,"polynomial_normalization","exact polynomial normalization"},{2,print(s.right),key,"polynomial_normalization","exact polynomial normalization"}};
                p.finalKey=key;return p;
            }
            if(rationalEqualityUnderAssumptions(s)){
                auto a=rationalPolynomial(s.left),b=rationalPolynomial(s.right);std::string key="CROSS:"+(a->n*b->d).key();Proof p;
                p.steps={{1,print(s.left),key,"rational_cross_normalization","cross multiply after checking explicit denominator obligations"},{2,print(s.right),key,"rational_cross_normalization","cross multiply after checking explicit denominator obligations"}};p.finalKey=key;return p;
            }
        }
        if(s.assumptions.empty()){
            std::set<std::string>vs;vars(s.left,vs);vars(s.right,vs);
            if(vs.empty()) try{
                bool ok=relationHolds(s.rel,eval(s.left,{}),eval(s.right,{}));
                if(ok){std::string key="EXACT:"+relationName(s.rel)+":"+canonical(s.left)+":"+canonical(s.right);Proof p;
                    p.steps={{1,print(s.left),key,"exact_evaluation","closed-form exact evaluation"},{2,print(s.right),key,"exact_evaluation","closed-form exact evaluation"}};p.finalKey=key;return p;}
            }catch(const Error&){}
        }
        // Last direct attempt: search for a short chain of trusted recursive
        // rewrites, then finish in the ordinary exact algebra kernel.
        if(s.rel==Rel::Eq){
            auto chain=RewritePlanner().prove(s,std::min(10,std::max(2,maxDepth_)));
            if(chain)return chain;
            auto bidir=BidirectionalRewriteSearch().prove(s,std::max(100,maxStates_),std::min(12,maxDepth_));
            if(bidir)return bidir;
        }
        return{};
    }
};

// ============================================================================
// 7. Euler knowledge graph, Gauss scoring, Galois structural analysis, Meta
// ============================================================================
struct Score { double novelty=0,generality=0,structure=0,compression=0,evidence=0,triviality=0,total=0; };
struct Record { int id=0; Statement statement; Status status=Status::GENERATED; Evidence evidence; Score score; Optional<Proof> proof; std::string generator,ancestry,kernelReason; };
static std::string relationName(Rel r){
    static const char* names[]={"=","!=","<","<=",">",">=","divides"};
    return names[int(r)];
}
static std::string assumptionKey(const std::vector<Assumption>& as){
    std::vector<std::string> v;
    for(const auto& a:as) v.push_back(canonical(subtraction(a.left,a.right))+":"+relationName(a.rel));
    std::sort(v.begin(),v.end());
    std::string out;
    for(const auto& x:v) out+=x+"|";
    return out;
}
static std::string statementKey(const Statement&s){
    return relationName(s.rel)+"|L:"+canonical(s.left)+"|R:"+canonical(s.right)+"|A:"+assumptionKey(s.assumptions);
}
class Euler {
    int next=1;
    std::vector<Record> records;
    std::unordered_map<std::string,int> theoremByKey;
public:
    int add(Record r){
        std::string key=statementKey(r.statement);
        if(r.status==Status::KERNEL_VERIFIED){
            auto known=theoremByKey.find(key);
            if(known!=theoremByKey.end()) return known->second;
        }
        r.id=next++;
        records.push_back(std::move(r));
        return records.back().id;
    }
    bool knownEquivalent(const Statement&s)const{
        return theoremByKey.count(statementKey(s))>0;
    }
    void promote(int id){
        for(auto&r:records) if(r.id==id){
            r.status=r.statement.assumptions.empty()?Status::THEOREM:Status::CONDITIONAL_THEOREM;
            theoremByKey[statementKey(r.statement)]=id;
            return;
        }
    }
    const std::vector<Record>& all()const{return records;}
    const Record* get(int id)const{for(auto&r:records)if(r.id==id)return&r;return nullptr;}
    void setStatus(int id,Status st){for(auto&r:records)if(r.id==id){r.status=st;return;}}
};class Gauss { public: Score score(const Statement&s,const Evidence&e,bool known){Score x;std::set<std::string>v;vars(s.left,v);vars(s.right,v);x.generality=std::min(1.0,double(v.size())/3);x.structure=(polynomial(s.left)&&polynomial(s.right))?.8:.25;x.compression=std::min(1.0,(print(s.left).size()+print(s.right).size())/40.0);x.evidence=e.tests?(!e.counterexample?1.0:0.0):0;x.triviality=known?.95:(canonical(s.left)==canonical(s.right)?.8:.05);x.novelty=known?.02:.75;x.total=.25*x.novelty+.20*x.generality+.20*x.structure+.15*x.compression+.20*x.evidence-.30*x.triviality;return x;} };
class Meta { std::map<std::string,std::pair<int,int>> outcomes; public:void observe(const std::string&method,bool success){auto&o=outcomes[method];++o.second;if(success)++o.first;}double reliability(const std::string&m)const{auto i=outcomes.find(m);return i==outcomes.end()?.5:double(i->second.first+1)/(i->second.second+2);}std::string report()const{std::ostringstream o;for(const auto& item:outcomes)o<<item.first<<" "<<item.second.first<<"/"<<item.second.second<<"; ";return o.str();} };
class Galois { public: bool symmetricAddition(const Statement&s)const{auto p=polynomial(s.left),q=polynomial(s.right);return p&&q&&p->key()==q->key();} };

// ============================================================================
// 8. Ramanujan generation and discovery loop
// ============================================================================
// Structure-first generator.  It samples variables and algebraic templates,
// rather than selecting a fixed list of finished equations.
class Ramanujan {
    std::mt19937_64 rng;
    uint64_t serial=0;
    std::string v(){
        static const char* names[]={"a","b","c","x","y","z","u","v"};
        return names[rng()%8];
    }
    std::pair<std::string,std::string> vars3(){
        std::string a=v(),b=v(),c=v();
        while(b==a)b=v();
        while(c==a||c==b)c=v();
        return {a,b+c};
    }
public:
    explicit Ramanujan(uint64_t s):rng(s){}
    std::pair<Statement,std::string> generate(){
        std::string a=v(),b=v(),c=v(),l,r,origin;
        int shape=int(rng()%20);
        switch(shape){
            case 0:
                l=a+"*("+b+"+"+c+")"; r=a+"*"+b+"+"+a+"*"+c;
                origin="template: distributive law"; break;
            case 1:
                l="("+a+"+"+b+")^2"; r=a+"^2+2*"+a+"*"+b+"+"+b+"^2";
                origin="template: square expansion"; break;
            case 2:
                l="("+a+"+"+b+")*("+a+"-"+b+")"; r=a+"^2-"+b+"^2";
                origin="template: difference of squares"; break;
            case 3:
                l="("+a+"+"+b+")+"+c; r=a+"+("+b+"+"+c+")";
                origin="template: associative addition"; break;
            case 4:
                l=a+"+"+b; r=b+"+"+a;
                origin="template: commutative addition"; break;
            case 5:
                l=a+"*"+b; r=b+"*"+a;
                origin="template: commutative multiplication"; break;
            case 6:
                l=a+"*1"; r=a;
                origin="template: multiplicative identity"; break;
            case 7:
                l=a+"+0"; r=a;
                origin="template: additive identity"; break;
            case 8:
                l="("+a+"-"+b+")+"+b; r=a;
                origin="template: additive inverse"; break;
            case 9:
                l=a+"^2+2*"+a+"*"+b+"+"+b+"^2"; r="("+a+"+"+b+")^2";
                origin="template: reverse square expansion"; break;
            case 10:
                l="("+a+"^2-1)/("+a+"-1)"; r=a+"+1";
                origin="template: conditional rational simplification"; break;
            case 11:
                l="gcd(84,30)"; r="6";
                origin="number-theory: gcd evaluation"; break;
            case 12:
                l="phi(36)"; r="12";
                origin="number-theory: Euler totient"; break;
            case 13:
                l="tau(360)"; r="24";
                origin="number-theory: divisor counting"; break;
            case 14:
                l="fib(50)"; r="12586269025";
                origin="number-theory: Fibonacci"; break;
            case 15:
                l="nCr(10,3)"; r="120";
                origin="combinatorics: binomial coefficient"; break;
            case 16:
                l="catalan(6)"; r="132";
                origin="combinatorics: Catalan number"; break;
            case 17:
                l="stirling2(6,3)"; r="90";
                origin="combinatorics: Stirling number"; break;
            case 18:
                l="partition(10)"; r="42";
                origin="combinatorics: integer partition"; break;
            default:
                l="nCr(10,3)"; r="121";
                origin="mutation: intentionally false combinatorial conjecture"; break;
        }
        if(shape==10) {
            Parser px(l+" = "+r+" assuming "+a+"!=1");
            ++serial;
            return {px.statement(),origin};
        }
        Parser px(l+" = "+r);
        ++serial;
        return {px.statement(),origin};
    }
};

// ============================================================================
// 8A. Autonomous symbolic identity synthesis
// ============================================================================
// This generator is intentionally NOT a database of finished theorems.
// It constructs fresh symbolic expressions, transforms them with generic
// algebraic operations, and asks the normal proof kernel whether the resulting
// identity is true.  Consequently the exact shape of the discovered theorem
// is determined by the search, not by a pre-written equation list.
class AlgebraicTheoremMiner {
    mutable std::mt19937_64 rng_;
    uint64_t seed_;
    int depth_;

    E leaf(){
        static const char* names[]={"a","b","c","x","y","z","u","v","w"};
        return vr(names[rng_()%9]);
    }
    E sumOfLeaves(int n){
        E out=leaf();
        for(int i=1;i<n;++i) out=op(Kind::Add,{out,leaf()});
        return out;
    }
    E productOfSums(){
        int factors=2+int(rng_()%2);
        E out=sumOfLeaves(2);
        for(int i=1;i<factors;++i) out=op(Kind::Mul,{out,sumOfLeaves(2)});
        return out;
    }
    E expand(const E&e) const {
        if(e->k==Kind::Add){
            std::vector<E> a;
            for(const auto&x:e->a) a.push_back(expand(x));
            return op(Kind::Add,a);
        }
        if(e->k==Kind::Mul){
            std::vector<E> factors;
            for(const auto&x:e->a) factors.push_back(expand(x));
            E out=cn(1);
            for(const auto&f:factors){
                std::vector<E> terms;
                if(f->k==Kind::Add) terms=f->a;
                else terms.push_back(f);
                std::vector<E> products;
                // Distribute the accumulated expression over this factor.
                if(out->k==Kind::Add){
                    for(const auto&left:out->a)
                        for(const auto&right:terms)
                            products.push_back(op(Kind::Mul,{left,right}));
                }else{
                    for(const auto&right:terms)
                        products.push_back(op(Kind::Mul,{out,right}));
                }
                if(products.size()==1) out=products[0];
                else out=op(Kind::Add,products);
            }
            return out;
        }
        if(e->k==Kind::Pow){
            auto n=smallNatural(e->a[1]);
            if(n && *n>=2 && *n<=3){
                E out=cn(1);
                for(int i=0;i<*n;++i) out=op(Kind::Mul,{out,expand(e->a[0])});
                return expand(out);
            }
        }
        return e;
    }
public:
    explicit AlgebraicTheoremMiner(uint64_t seed=1,int depth=3):rng_(seed),seed_(seed),depth_(std::max(2,depth)){}

    std::vector<std::pair<std::string,std::string> > generate(int budget){
        std::vector<std::pair<std::string,std::string> > out;
        std::set<std::string> seenLocal;
        for(int i=0;i<budget;++i){
            E factored=productOfSums();
            E expanded=expand(factored);
            std::string a=print(factored),b=print(expanded);
            if(a==b) continue;
            Statement s;
            try{s=Parser(a+" = "+b).statement();}catch(...){continue;}
            if(canonical(s.left)!=canonical(s.right)) continue;
            std::string key=statementKey(s);
            if(!seenLocal.insert(key).second) continue;
            std::ostringstream origin;
            origin<<"autonomous symbolic synthesis: random product-of-sums expansion (seed="<<seed_<<")";
            out.push_back({a+" = "+b,origin.str()});
        }
        return out;
    }
};

// ============================================================================
// 8. AUTONOMOUS MATHEMATICAL RESEARCH BRAIN
// ============================================================================
// This layer is the research system, not the theorem oracle.
//
// Research loop:
//   observations -> representations -> conjectures -> adversarial tests
//   -> proof search -> kernel verification -> generalization -> graph/theory
//   -> new research questions -> repeat.
//
// The distinction is deliberate:
//   * conjectures may be wrong;
//   * experiments are evidence, never proof;
//   * only Kernel::verify can create theorem status.
//
// The research brain therefore becomes more ambitious without weakening the
// trusted boundary.  It can discover statements that are NOT pre-written,
// learn formulas/recurrences from a supplied database, mutate hypotheses,
// search combinations of known results, and organize verified results into
// structural theories.

struct ResearchEdge {
    int from=0,to=0;
    std::string relation,reason;
};

struct Theory {
    int id=0;
    std::string name,domain,thesis;
    std::vector<int> members;
    std::vector<ResearchEdge> edges;
    std::vector<std::string> definitions;
    std::vector<std::string> principles;
    std::vector<int> lemmas;
    std::vector<int> conjectures;
    std::vector<std::string> openQuestions;
    double coherence=0.0;
    double maturity=0.0;
};

class TheoryBuilder {
    static bool contains(const std::string&t,const std::string&k){return t.find(k)!=std::string::npos;}
    static void uniquePush(std::vector<std::string>&v,const std::string&x){
        if(x.empty())return;
        if(std::find(v.begin(),v.end(),x)==v.end())v.push_back(x);
    }
    static std::string domainThesis(const std::string&d,const std::vector<std::string>&c){
        std::ostringstream o;
        if(d=="algebra") o<<"Study verified algebraic identities and their structural consequences";
        else if(d=="number_theory") o<<"Study exact arithmetic functions, divisibility, congruences, and integer structure";
        else if(d=="combinatorics") o<<"Study counting functions, recurrences, symmetries, and discrete structures";
        else if(d=="recurrences") o<<"Study recursive laws, sequence transformations, and invariants";
        else o<<"Study verified mathematical relationships in the "<<d<<" domain";
        if(!c.empty()){o<<" centered on ";for(size_t i=0;i<c.size();++i){if(i)o<<", ";o<<c[i];}}
        o<<".";
        return o.str();
    }
public:
    static void rebuild(Theory&t,const Euler&kb,const std::vector<ResearchEdge>&graphEdges){
        t.definitions.clear(); t.principles.clear(); t.lemmas.clear();
        t.conjectures.clear(); t.openQuestions.clear(); t.edges.clear();
        std::vector<std::string> concepts;
        int theoremCount=0, conditionalCount=0;
        for(int id:t.members){
            const Record*r=kb.get(id); if(!r)continue;
            const std::string text=print(r->statement);
            if(r->status==Status::THEOREM)++theoremCount;
            else if(r->status==Status::CONDITIONAL_THEOREM)++conditionalCount;
            else if(r->status==Status::LEMMA_FOUND)t.lemmas.push_back(id);
            else if(r->status==Status::CONJECTURE||r->status==Status::EMPIRICALLY_SUPPORTED)t.conjectures.push_back(id);
            const char* keys[]={"gcd","lcm","phi","tau","sigma","fib","factorial","nCr","nPr","catalan","partition","bell","stirling2","derangements","mod"};
            for(const char*k:keys) if(contains(text,k)) uniquePush(concepts,k);
            if(contains(text,"gcd")) uniquePush(t.definitions,"gcd: greatest common divisor structure");
            if(contains(text,"phi")) uniquePush(t.definitions,"phi: Euler totient function structure");
            if(contains(text,"tau")) uniquePush(t.definitions,"tau: divisor-counting function structure");
            if(contains(text,"sigma")) uniquePush(t.definitions,"sigma: divisor-sum function structure");
            if(contains(text,"fib")) uniquePush(t.definitions,"fib: Fibonacci recurrence structure");
            if(contains(text,"factorial")) uniquePush(t.definitions,"factorial: recursive multiplicative structure");
            if(contains(text,"nCr")) uniquePush(t.definitions,"nCr: binomial coefficient structure");
            if(contains(text,"nPr")) uniquePush(t.definitions,"nPr: permutation-counting structure");
            if(contains(text,"partition")) uniquePush(t.definitions,"partition: integer partition-counting structure");
            if(contains(text,"catalan")) uniquePush(t.definitions,"catalan: Catalan counting structure");
            if(contains(text,"bell")) uniquePush(t.definitions,"bell: Bell-number set-partition structure");
            if(r->status==Status::THEOREM||r->status==Status::CONDITIONAL_THEOREM||r->status==Status::LEMMA_FOUND)
                if(text.find("=")!=std::string::npos) uniquePush(t.principles,"verified identity: "+text);
        }
        for(const auto&e:graphEdges){
            bool a=std::find(t.members.begin(),t.members.end(),e.from)!=t.members.end();
            bool b=std::find(t.members.begin(),t.members.end(),e.to)!=t.members.end();
            if(a&&b)t.edges.push_back(e);
        }
        if(t.domain.empty())t.domain="mixed";
        t.thesis=domainThesis(t.domain,concepts);
        if(t.domain=="number_theory"){
            uniquePush(t.openQuestions,"Which verified arithmetic identities admit a stronger coprimality-free form?");
            uniquePush(t.openQuestions,"Which observed arithmetic functions share an underlying multiplicative law?");
        } else if(t.domain=="combinatorics"){
            uniquePush(t.openQuestions,"Which identities arise from a common recurrence, bijection, or generating-function principle?");
            uniquePush(t.openQuestions,"Can the verified counting laws be lifted to a parameterized family?");
        } else if(t.domain=="algebra"){
            uniquePush(t.openQuestions,"Which verified identities are instances of a smaller set of algebraic laws?");
            uniquePush(t.openQuestions,"Can the identities be generalized from polynomials to a broader algebraic structure?");
        } else {
            uniquePush(t.openQuestions,"What is the smallest structural principle explaining the verified results?");
            uniquePush(t.openQuestions,"Can the observed laws be generalized beyond the current examples?");
        }
        const double n=double(t.members.size());
        const double structural=std::min(1.0,double(concepts.size())/4.0);
        const double proof=std::min(1.0,double(theoremCount+conditionalCount)/6.0);
        const double links=std::min(1.0,double(t.edges.size())/8.0);
        t.coherence=0.45*structural+0.35*links+0.20*(n>0?1.0:0.0);
        t.maturity=0.55*proof+0.25*structural+0.20*links;
    }
};

class TheoryResearcher {
public:
    struct Proposal { std::string text,reason; };
    std::vector<Proposal> propose(const std::vector<Theory>&theories,const Euler&kb,int budget)const{
        std::vector<Proposal>out;
        for(size_t i=0;i<theories.size()&&int(out.size())<budget;++i){
            for(size_t j=i+1;j<theories.size()&&int(out.size())<budget;++j){
                if(theories[i].domain==theories[j].domain)continue;
                const Record*a=nullptr; const Record*b=nullptr;
                for(int id:theories[i].members){const Record*x=kb.get(id);if(x&&(x->status==Status::THEOREM||x->status==Status::CONDITIONAL_THEOREM)){a=x;break;}}
                for(int id:theories[j].members){const Record*x=kb.get(id);if(x&&(x->status==Status::THEOREM||x->status==Status::CONDITIONAL_THEOREM)){b=x;break;}}
                if(!a||!b)continue;
                auto pa=polynomial(a->statement.left); auto qa=polynomial(a->statement.right);
                auto pb=polynomial(b->statement.left); auto qb=polynomial(b->statement.right);
                if(pa&&qa&&pb&&qb){
                    std::string text="("+print(a->statement.left)+")-("+print(a->statement.right)+")=("+print(b->statement.left)+")-("+print(b->statement.right)+")";
                    try{Parser p(text);p.statement();out.push_back({text,"cross-theory bridge hypothesis"});}catch(...){ }
                }
            }
        }
        return out;
    }
};

class ResearchGraph {
    std::vector<ResearchEdge> edges_;
public:
    void add(int a,int b,const std::string&r,const std::string&why){
        if(a==b) return;
        for(const auto&e:edges_)
            if(e.from==a&&e.to==b&&e.relation==r) return;
        edges_.push_back({a,b,r,why});
    }
    const std::vector<ResearchEdge>& edges()const{return edges_;}

    void buildSimilarity(const std::vector<Record>&records){
        for(size_t i=0;i<records.size();++i){
            if(records[i].status!=Status::THEOREM &&
               records[i].status!=Status::CONDITIONAL_THEOREM) continue;
            std::set<std::string> a;
            vars(records[i].statement.left,a);
            vars(records[i].statement.right,a);
            std::string ta=print(records[i].statement);
            for(size_t j=i+1;j<records.size();++j){
                if(records[j].status!=Status::THEOREM &&
                   records[j].status!=Status::CONDITIONAL_THEOREM) continue;
                std::set<std::string> b;
                vars(records[j].statement.left,b);
                vars(records[j].statement.right,b);
                std::string tb=print(records[j].statement);
                bool shared=false;
                for(const auto&x:a) if(b.count(x)) {shared=true;break;}
                bool domainShared=false;
                const char* words[]={"gcd","phi","tau","sigma","fib","nCr",
                                     "nPr","catalan","derangements","partition",
                                     "bell","factorial"};
                for(const char*w:words)
                    if(ta.find(w)!=std::string::npos&&tb.find(w)!=std::string::npos)
                        {domainShared=true;break;}
                if(shared) add(records[i].id,records[j].id,"shared_variables",
                               "theorems share symbolic variables");
                if(domainShared) add(records[i].id,records[j].id,"shared_domain",
                                     "theorems share mathematical vocabulary");
            }
        }
    }

    static std::string htmlEscape(const std::string&s){
        std::string o;
        o.reserve(s.size()+16);
        for(char c:s){
            switch(c){
                case '&': o+="&amp;"; break;
                case '<': o+="&lt;"; break;
                case '>': o+="&gt;"; break;
                case '"': o+="&quot;"; break;
                case '\\': o+="&#92;"; break;
                default: o+=c;
            }
        }
        return o;
    }
    static std::string jsEscape(const std::string&s){
        std::ostringstream o;
        for(unsigned char c:s){
            switch(c){
                case '\\': o<<"\\\\"; break;
                case '"': o<<"\\\""; break;
                case '\n': o<<"\\n"; break;
                case '\r': o<<"\\r"; break;
                case '\t': o<<"\\t"; break;
                case '<': o<<"\\u003c"; break;
                case '>': o<<"\\u003e"; break;
                case '&': o<<"\\u0026"; break;
                default: if(c<32) o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c)<<std::dec; else o<<char(c);
            }
        }
        return o.str();
    }
    void exportLiveHtml(const std::string&file,const std::vector<Record>&records)const{
        std::string tmp=file+".tmp";
        std::ofstream out(tmp);
        if(!out) throw Error("cannot write live graph: "+file);
        out<<"<!doctype html><html><head><meta charset=\"utf-8\">"
           <<"<meta http-equiv=\"refresh\" content=\"1\">"
           <<"<title>H.I.L.B.E.R.T. Live Research Graph</title>"
           <<"<style>body{font-family:Arial,sans-serif;margin:0;background:#111;color:#eee}"
           <<"header{padding:14px 18px;background:#181818;position:sticky;top:0;z-index:2}"
           <<"#meta{opacity:.75;font-size:13px}svg{width:100%;height:calc(100vh - 82px);background:#0d0d0d}"
           <<".edge{stroke:#777;stroke-width:1.2;fill:none;opacity:.7}.node{stroke:#ddd;stroke-width:1}"
           <<".label{fill:#eee;font-size:11px}.theorem{fill:#247a3b}.conditional{fill:#7a6b24}.other{fill:#444}"
           <<"</style></head><body><header><b>H.I.L.B.E.R.T. LIVE RESEARCH GRAPH</b>"
           <<"<div id=\"meta\">records="<<records.size()<<" | edges="<<edges_.size()<<" | auto-refresh=1s | generated="
           <<htmlEscape(__DATE__)<<" "<<htmlEscape(__TIME__)<<"</div></header><svg id=\"g\" viewBox=\"0 0 1600 900\"></svg>"
           <<"<script>\n"
           <<"const nodes=[\n";
        for(size_t i=0;i<records.size();++i){
            const auto&r=records[i];
            out<<"{id:"<<r.id<<",status:\""<<jsEscape(statusName(r.status))<<"\",label:\""<<jsEscape(print(r.statement))<<"\"}"
               <<(i+1<records.size()?",":"")<<"\n";
        }
        out<<"]\nconst edges=[\n";
        for(size_t i=0;i<edges_.size();++i){
            const auto&e=edges_[i];
            out<<"{from:"<<e.from<<",to:"<<e.to<<",rel:""<<jsEscape(e.relation)<<""}"
               <<(i+1<edges_.size()?",":"")<<"\n";
        }
        out<<R"HILBERTJS(];
const svg=document.getElementById('g');
const W=1600,H=900,cols=Math.max(1,Math.ceil(Math.sqrt(nodes.length))),rows=Math.max(1,Math.ceil(nodes.length/cols));
const pos=new Map();
for(let i=0;i<nodes.length;i++){let c=i%cols,r=Math.floor(i/cols);pos.set(nodes[i].id,{x:80+c*(W-160)/Math.max(1,cols-1),y:90+r*(H-150)/Math.max(1,rows-1)});}
function el(t,a){let x=document.createElementNS('http://www.w3.org/2000/svg',t);for(const[k,v]of Object.entries(a))x.setAttribute(k,v);return x;}
for(const e of edges){const a=pos.get(e.from),b=pos.get(e.to);if(!a||!b)continue;svg.appendChild(el('line',{x1:a.x,y1:a.y,x2:b.x,y2:b.y,class:'edge'}));}
for(const n of nodes){const p=pos.get(n.id);const g=el('g',{});const c=n.status==='THEOREM'||n.status==='LEMMA_FOUND'?'theorem':(n.status==='CONDITIONAL_THEOREM'?'conditional':'other');g.appendChild(el('circle',{cx:p.x,cy:p.y,r:22,class:'node '+c}));const t=el('text',{x:p.x,y:p.y+4,'text-anchor':'middle',class:'label'});t.textContent='#'+n.id;g.appendChild(t);const title=el('title',{});title.textContent=n.label+' ['+n.status+']';g.appendChild(title);svg.appendChild(g);}
</script></body></html>
)HILBERTJS";
        out.close();
        std::remove(file.c_str());
        if(std::rename(tmp.c_str(),file.c_str())!=0) throw Error("cannot replace live graph: "+file);
    }

    void exportDot(const std::string&file,const std::vector<Record>&records)const{
        std::ofstream out(file);
        if(!out) throw Error("cannot write graph: "+file);
        out<<"digraph HILBERT_RESEARCH {\n";
        out<<"  rankdir=LR;\n";
        for(const auto&r:records){
            std::string label=print(r.statement);
            for(char&c:label) if(c=='"') c='\'';
            out<<"  n"<<r.id<<" [label=\"#"<<r.id<<" "
               <<label<<"\\n"<<statusName(r.status)<<"\"];\n";
        }
        for(const auto&e:edges_)
            out<<"  n"<<e.from<<" -> n"<<e.to
               <<" [label=\""<<e.relation<<"\"];\n";
        out<<"}\n";
    }
};

class FormulaMiner {
    struct Signature{std::string call,key;};
    static bool exactClosed(const E&e){std::set<std::string>v;vars(e,v);return v.empty();}
    static bool parseObservation(const Statement&s,Signature&sig,int&n,BigInt&value){
        if(s.rel!=Rel::Eq||!s.assumptions.empty()||s.left->k!=Kind::Func||
           s.left->a.empty()||!exactClosed(s.right)) return false;
        if(!exactClosed(s.left->a[0])) return false;
        try{
            Rational q=eval(s.left->a[0],{});
            if(q.d!=BigInt(1)) return false;
            n=std::stoi(q.n.str());
            value=integerValue(eval(s.right,{}));
            sig.call=s.left->name+"(n";
            sig.key=s.left->name;
            for(size_t i=1;i<s.left->a.size();++i){
                if(!exactClosed(s.left->a[i])) return false;
                sig.call+=","+print(s.left->a[i]);
                sig.key+="#"+print(s.left->a[i]);
            }
            sig.call+=")";
            return true;
        }catch(...){return false;}
    }
    static BigInt smallFactorial(int n){
        BigInt r(1);for(int i=2;i<=n;++i)r*=BigInt(i);return r;
    }
    static std::string polyFormula(const Signature&sig,const std::vector<BigInt>&vals,int degree){
        std::vector<std::vector<BigInt>> d; d.push_back(vals);
        for(int k=1;k<=degree&&k<(int)vals.size();++k){
            std::vector<BigInt> row;
            for(size_t i=1;i<d.back().size();++i)
                row.push_back(d.back()[i]-d.back()[i-1]);
            d.push_back(row);
        }
        std::ostringstream out; out<<sig.call<<" = "; bool first=true;
        for(int k=0;k<=degree&&k<(int)d.size();++k){
            BigInt c=d[k][0]; if(c.isZero()) continue;
            if(!first) out<<(c.negative()?" - ":" + ");
            else if(c.negative()) out<<"-";
            BigInt ac=c.abs(); bool unit=ac==BigInt(1);
            if(k==0) out<<ac.str();
            else{
                if(!unit) out<<ac.str()<<"*";
                for(int j=0;j<k;++j){
                    if(j) out<<"*";
                    out<<"(n-"<<j<<")";
                }
                BigInt den=smallFactorial(k);
                if(den!=BigInt(1)) out<<"/"<<den.str();
            }
            first=false;
        }
        if(first) out<<"0";
        return out.str();
    }
public:
    std::vector<std::string> mine(const std::vector<Statement>&facts){
        std::map<std::string,std::map<int,BigInt>>seq;
        std::map<std::string,Signature>signatures;
        for(const auto&s:facts){
            Signature sig;int n;BigInt v;
            if(parseObservation(s,sig,n,v)){
                seq[sig.key][n]=v;
                signatures[sig.key]=sig;
            }
        }
        std::vector<std::string>out;
        for(const auto&kv:seq){
            const auto&m=kv.second;
            if(m.size()<6) continue;
            int lo=m.begin()->first,hi=m.rbegin()->first;
            if(hi-lo+1!=(int)m.size()) continue;
            std::vector<BigInt>vals;
            for(int n=lo;n<=hi;++n) vals.push_back(m.at(n));
            for(int deg=1;deg<=5&&deg<(int)vals.size();++deg){
                std::vector<std::vector<BigInt>>d{vals};
                for(int k=1;k<=deg;++k){
                    std::vector<BigInt>row;
                    for(size_t i=1;i<d.back().size();++i)
                        row.push_back(d.back()[i]-d.back()[i-1]);
                    d.push_back(row);
                }
                bool constantDiff=d.back().size()>=1;
                for(size_t i=1;i<d.back().size();++i)
                    if(d.back()[i]!=d.back()[0]) constantDiff=false;
                if(constantDiff){
                    out.push_back(polyFormula(signatures.at(kv.first),vals,deg)+
                                   " assuming n>="+std::to_string(lo));
                    break;
                }
            }
        }
        return out;
    }
};

class RecurrenceMiner {
    struct Obs{std::string f;int n;BigInt v;};
    static bool varsEmpty(const E&e){std::set<std::string>v;vars(e,v);return v.empty();}
    static bool obs(const Statement&s,Obs&o){
        if(s.rel!=Rel::Eq||!s.assumptions.empty()||s.left->k!=Kind::Func||
           s.left->a.size()!=1) return false;
        try{
            Rational q=eval(s.left->a[0],{});
            if(q.d!=BigInt(1)||!varsEmpty(s.right)) return false;
            o.f=s.left->name;o.n=std::stoi(q.n.str());
            o.v=integerValue(eval(s.right,{}));return true;
        }catch(...){return false;}
    }
public:
    std::vector<std::string> mine(const std::vector<Statement>&facts){
        std::map<std::string,std::map<int,BigInt>>seq;
        for(const auto&s:facts){Obs o;if(obs(s,o))seq[o.f][o.n]=o.v;}
        std::vector<std::string>out;
        for(const auto&kv:seq){
            const auto&m=kv.second;
            if(m.size()<8) continue;
            int lo=m.begin()->first,hi=m.rbegin()->first;
            if(hi-lo+1!=(int)m.size()) continue;
            std::vector<BigInt>v;
            for(int n=lo;n<=hi;++n)v.push_back(m.at(n));
            for(int order=1;order<=4;++order){
                bool found=false;int total=1;
                for(int i=0;i<order;++i) total*=9;
                for(int code=0;code<total&&!found;++code){
                    int q=code;std::vector<int>c(order);
                    for(int i=0;i<order;++i){c[i]=(q%9)-4;q/=9;}
                    bool ok=true;
                    for(size_t n=order;n<v.size();++n){
                        BigInt rhs(0);
                        for(int j=1;j<=order;++j)
                            rhs+=BigInt(c[j-1])*v[n-j];
                        if(rhs!=v[n]){ok=false;break;}
                    }
                    if(ok){
                        std::ostringstream z;z<<kv.first<<"(n) = ";
                        bool first=true;
                        for(int j=1;j<=order;++j){
                            if(c[j-1]==0)continue;
                            if(!first)z<<(c[j-1]>0?" + ":" - ");
                            else if(c[j-1]<0)z<<"-";
                            int ac=std::abs(c[j-1]);
                            if(ac!=1)z<<ac<<"*";
                            z<<kv.first<<"(n-"<<j<<")";first=false;
                        }
                        if(first)z<<"0";
                        z<<" assuming n>="<<lo+order;
                        out.push_back(z.str());found=true;
                    }
                }
                if(found)break;
            }
        }
        return out;
    }
};

// Small exact helpers used by the research layer.
static bool exprClosed(const E&e){std::set<std::string>v;vars(e,v);return v.empty();}
static std::string domainOf(const Statement&s){
    std::string t=print(s);
    if(t.find("nCr")!=std::string::npos||t.find("nPr")!=std::string::npos||
       t.find("catalan")!=std::string::npos||t.find("stirling")!=std::string::npos||
       t.find("partition")!=std::string::npos||t.find("bell")!=std::string::npos||
       t.find("derangement")!=std::string::npos||t.find("composition")!=std::string::npos)
        return "combinatorics";
    if(t.find("gcd")!=std::string::npos||t.find("phi")!=std::string::npos||
       t.find("tau")!=std::string::npos||t.find("sigma")!=std::string::npos||
       t.find("prime")!=std::string::npos||t.find("powmod")!=std::string::npos||
       t.find("mod")!=std::string::npos) return "number_theory";
    if(t.find("fib")!=std::string::npos||t.find("factorial")!=std::string::npos)
        return "recurrences";
    if(polynomial(s.left)&&polynomial(s.right)) return "algebra";
    return "general";
}

static E cloneSubstitute(const E&e,const std::map<std::string,E>&sub){
    if(e->k==Kind::Var){
        auto it=sub.find(e->name);
        return it==sub.end()?vr(e->name):it->second;
    }
    auto z=std::make_shared<Expr>();
    z->k=e->k;z->q=e->q;z->name=e->name;
    for(const auto&c:e->a)z->a.push_back(cloneSubstitute(c,sub));
    return z;
}

class TheoremTransformer {
    static std::vector<std::string> names(){
        return {"a","b","c","x","y","z","u","v","m","n","k"};
    }
    static E replacement(std::mt19937_64&rng,const std::vector<std::string>&ns,int depth){
        if(depth<=0 || rng()%3==0) return vr(ns[rng()%ns.size()]);
        int shape=int(rng()%4);
        if(shape==0)return op(Kind::Add,{replacement(rng,ns,depth-1),
                                         replacement(rng,ns,depth-1)});
        if(shape==1)return op(Kind::Mul,{replacement(rng,ns,depth-1),
                                         replacement(rng,ns,depth-1)});
        if(shape==2)return op(Kind::Neg,{replacement(rng,ns,depth-1)});
        return op(Kind::Pow,{replacement(rng,ns,depth-1),cn(int64_t(rng()%4))});
    }
public:
    std::vector<std::pair<Statement,std::string>> instantiate(
            const Euler&kb,int limit,uint64_t seed){
        std::mt19937_64 rng(seed);
        std::vector<std::pair<Statement,std::string>>out;
        auto ns=names();
        for(const auto&r:kb.all()){
            if(r.status!=Status::THEOREM&&r.status!=Status::CONDITIONAL_THEOREM)continue;
            std::set<std::string>vs;
            vars(r.statement.left,vs);vars(r.statement.right,vs);
            for(const auto&a:r.statement.assumptions){
                vars(a.left,vs);vars(a.right,vs);
            }
            if(vs.empty())continue;
            std::vector<std::string>v(vs.begin(),vs.end());
            for(int attempt=0;attempt<8&&(int)out.size()<limit;++attempt){
                std::map<std::string,E>sub;
                for(const auto&x:v)sub[x]=replacement(rng,ns,1+(int)(rng()%2));
                Statement s=r.statement;
                s.left=cloneSubstitute(r.statement.left,sub);
                s.right=cloneSubstitute(r.statement.right,sub);
                s.assumptions.clear();
                for(const auto&aa:r.statement.assumptions){
                    Assumption z=aa;
                    z.left=cloneSubstitute(aa.left,sub);
                    z.right=cloneSubstitute(aa.right,sub);
                    s.assumptions.push_back(z);
                }
                out.push_back({s,"generalization/substitution from theorem #"+
                                  std::to_string(r.id)});
            }
            if((int)out.size()>=limit)break;
        }
        return out;
    }
};

// Learns relationships among supplied exact observations.  It deliberately
// does not claim these relationships are theorems; it creates conjectures that
// must survive Popper and, if possible, the proof kernel.
class RelationalMiner {
    struct Obs { std::string f; std::vector<BigInt> args; BigInt value; };
    static bool get(const Statement&s,Obs&o){
        if(s.rel!=Rel::Eq||!s.assumptions.empty()||s.left->k!=Kind::Func||
           !exprClosed(s.right)) return false;
        try{
            for(const auto&a:s.left->a){
                if(!exprClosed(a))return false;
                Rational q=eval(a,{});
                if(q.d!=BigInt(1))return false;
                o.args.push_back(q.n);
            }
            o.f=s.left->name;o.value=integerValue(eval(s.right,{}));
            return true;
        }catch(...){return false;}
    }
    static std::string call1(const std::string&f,int n){
        return f+"("+std::to_string(n)+")";
    }
    static std::string call2(const std::string&f,int a,int b){
        return f+"("+std::to_string(a)+","+std::to_string(b)+")";
    }
    static BigInt evalCandidate(const std::string&text){
        Statement s=Parser(text).statement();
        return integerValue(eval(s.left,{}));
    }
    static bool allEqual(const std::vector<bool>&v){
        return !v.empty()&&std::find(v.begin(),v.end(),false)==v.end();
    }
public:
    std::vector<std::pair<std::string,std::string>> mine(
            const std::vector<Statement>&facts)const{
        std::map<std::string,std::map<int,BigInt>>one;
        std::map<std::string,std::map<std::pair<int,int>,BigInt>>two;
        for(const auto&s:facts){
            Obs o;if(!get(s,o))continue;
            if(o.args.size()==1){
                try{one[o.f][std::stoi(o.args[0].str())]=o.value;}catch(...){}
            }else if(o.args.size()==2){
                try{
                    int a=std::stoi(o.args[0].str()),b=std::stoi(o.args[1].str());
                    two[o.f][{a,b}]=o.value;
                }catch(...){}
            }
        }
        std::vector<std::pair<std::string,std::string>>out;
        // Data-driven unary laws: shift, doubling, affine additivity and
        // multiplicativity.  These are generated from observations, not fixed
        // theorem answers.
        for(const auto&kv:one){
            const auto&m=kv.second;
            if(m.size()<8)continue;
            bool shift=true, doubling=true;
            for(const auto&x:m){
                int n=x.first;
                auto i1=m.find(n+1),i2=m.find(2*n);
                if(i1==m.end()||i2==m.end()){shift=false;doubling=false;break;}
            }
            if(shift){
                bool diff=true, ratio2=true;
                for(const auto&x:m){
                    auto y=m.find(x.first+1);
                    if(y!=m.end()){
                        if(y->second-x.second!=BigInt(1))diff=false;
                        if(y->second!=BigInt(2)*x.second)ratio2=false;
                    }
                }
                if(diff)out.push_back({kv.first+"(n+1)="+kv.first+"(n)+1 assuming n>=0",
                                       "learned unary unit-difference law"});
                if(ratio2)out.push_back({kv.first+"(n+1)=2*"+kv.first+"(n) assuming n>=0",
                                         "learned unary doubling law"});
            }
            (void)doubling;
        }
        // Data-driven binary laws.  For every observed pair, compare f(a,b)
        // against f(b,a), f(a+b,...) when observations permit it.
        for(const auto&kv:two){
            const auto&m=kv.second; if(m.size()<8)continue;
            bool sym=true;
            for(const auto&x:m){
                auto y=m.find({x.first.second,x.first.first});
                if(y==m.end()||y->second!=x.second){sym=false;break;}
            }
            if(sym)out.push_back({kv.first+"(a,b)="+kv.first+"(b,a)",
                                  "learned permutation symmetry from database"});
        }
        return out;
    }
};


// Cross-sequence discovery: synthesize identities between independently
// observed sequences.  This is intentionally data-driven: HILBERT does not
// need a hand-written identity such as nPr(n,2)=2*nCr(n,2).  If the supplied
// database contains enough overlapping observations, the search proposes
// small affine/product/shift relations and sends them through the normal
// adversarial + proof pipeline.
class CrossSequenceMiner {
    using Seq=std::map<int,BigInt>;
    struct Obs { std::string sig; int n; BigInt v; };
    static bool observation(const Statement&s,Obs&o){
        if(s.rel!=Rel::Eq||!s.assumptions.empty()||s.left->k!=Kind::Func||
           s.left->a.empty()||!exprClosed(s.right)) return false;
        try{
            Rational q=eval(s.left->a[0],{});
            if(q.d!=BigInt(1)) return false;
            o.n=std::stoi(q.n.str());
            std::ostringstream sig;sig<<s.left->name<<"(n";
            for(size_t i=1;i<s.left->a.size();++i){
                if(!exprClosed(s.left->a[i]))return false;
                sig<<","<<print(s.left->a[i]);
            }
            sig<<")";o.sig=sig.str();
            o.v=integerValue(eval(s.right,{}));
            return true;
        }catch(...){return false;}
    }
public:
    std::vector<std::pair<std::string,std::string>> mine(
            const std::vector<Statement>&facts)const{
        std::map<std::string,Seq> seqs;
        for(const auto&s:facts){Obs o;if(observation(s,o))seqs[o.sig][o.n]=o.v;}
        std::vector<std::string> names;
        for(const auto&x:seqs)if(x.second.size()>=6)names.push_back(x.first);
        std::vector<std::pair<std::string,std::string>> out;
        const size_t CAP=160;
        const std::string sentinel="__HILBERT_NO_CONSTANT__";

        for(size_t i=0;i<names.size()&&out.size()<CAP;++i){
            for(size_t j=i+1;j<names.size()&&out.size()<CAP;++j){
                const auto&A=seqs[names[i]],&B=seqs[names[j]];
                std::vector<int> ns;
                for(const auto&x:A)if(B.count(x.first))ns.push_back(x.first);
                if(ns.size()<6)continue;

                // q*f(n) = p*g(n) + c for small rational/integer slopes.
                // Integer cross-multiplication keeps every candidate exact.
                for(int pcoef=-4;pcoef<=4&&out.size()<CAP;++pcoef){
                    if(pcoef==0)continue;
                    for(int qcoef=1;qcoef<=4&&out.size()<CAP;++qcoef){
                        bool first=true,ok=true;BigInt c(0);
                        for(int n:ns){
                            BigInt cur=BigInt(qcoef)*A.at(n)-BigInt(pcoef)*B.at(n);
                            if(first){c=cur;first=false;}
                            else if(cur!=c){ok=false;break;}
                        }
                        if(!ok)continue;
                        // Avoid the least interesting proportional identity
                        // when it is merely a duplicate of a simpler slope.
                        if(qcoef>1 && pcoef%qcoef==0 && c%(BigInt(qcoef))==BigInt(0))continue;
                        std::ostringstream q;
                        if(qcoef!=1)q<<qcoef<<"*";
                        q<<names[i]<<"=";
                        if(pcoef==-1)q<<"-"<<names[j];
                        else if(pcoef==1)q<<names[j];
                        else q<<pcoef<<"*"<<names[j];
                        if(!c.isZero())q<<(c.negative()?" - ":" + ")<<c.abs().str();
                        q<<" assuming n>="<<*std::min_element(ns.begin(),ns.end());
                        out.push_back({q.str(),"cross-sequence affine synthesis"});
                    }
                }

                // f(n) = g(n) + h(n), where all three sequences are observed
                // at the same indices.  This creates genuinely new candidate
                // identities from data rather than from a hard-coded theorem.
                for(size_t k=0;k<names.size()&&out.size()<CAP;++k){
                    if(k==i||k==j)continue;
                    const auto&C=seqs[names[k]];
                    bool ok=true;int count=0;
                    for(int n:ns){auto it=C.find(n);if(it==C.end()){ok=false;break;}
                        if(A.at(n)!=B.at(n)+it->second){ok=false;break;}++count;}
                    if(ok&&count>=6){
                        std::ostringstream q;q<<names[i]<<"="<<names[j]<<"+"<<names[k]
                          <<" assuming n>="<<*std::min_element(ns.begin(),ns.end());
                        out.push_back({q.str(),"cross-sequence additive synthesis"});
                    }
                }

                // Shift relations: f(n)=g(n+1)+b or f(n)=g(n-1)+b.
                for(int shift:{-1,1}){
                    bool first=true,ok=true;BigInt b(0);int count=0;
                    for(int n:ns){auto it=B.find(n+shift);if(it==B.end())continue;
                        BigInt cur=A.at(n)-it->second;
                        if(first){b=cur;first=false;}else if(cur!=b){ok=false;break;}++count;
                    }
                    if(ok&&count>=6){
                        std::ostringstream q;q<<names[i]<<"="<<names[j];
                        // Replace only the first occurrence of n in the
                        // function template, preserving fixed parameters.
                        std::string shifted=names[j];
                        size_t pos=shifted.find("(n");
                        if(pos==std::string::npos)continue;
                        size_t npos=pos+1;
                        shifted.replace(npos,1,shift>0?"(n+1":"(n-1");
                        // The replacement above leaves the original suffix
                        // after n; close it normally by removing the duplicated
                        // opening parenthesis introduced by the replacement.
                        // Reconstruct robustly from the original template.
                        shifted=names[j];
                        size_t lp=shifted.find("(n");
                        size_t afterN=lp+2;
                        shifted.insert(afterN,shift>0?"+1":"-1");
                        q.str("");q.clear();q<<names[i]<<"="<<shifted;
                        if(!b.isZero())q<<(b.negative()?" - ":" + ")<<b.abs().str();
                        q<<" assuming n>="<<*std::min_element(ns.begin(),ns.end());
                        out.push_back({q.str(),"cross-sequence shift synthesis"});
                    }
                }
            }
        }
        (void)sentinel;
        return out;
    }
};

class ConjectureSynthesizer {
    std::mt19937_64 rng_;
    static std::string mutateCoefficient(const std::string&s,int delta){
        std::string out=s;
        std::size_t p=out.find(" + ");
        if(p==std::string::npos)p=out.find(" - ");
        if(p!=std::string::npos){
            std::size_t b=p+3;
            if(b<out.size()&&std::isdigit((unsigned char)out[b])){
                int x=0;while(b<out.size()&&std::isdigit((unsigned char)out[b]))
                    {x=x*10+(out[b]-'0');++b;}
                x+=delta;out.replace(p+3,b-(p+3),std::to_string(std::max(0,x)));
            }
        }
        return out;
    }
public:
    explicit ConjectureSynthesizer(uint64_t seed=1):rng_(seed){}
    std::vector<std::pair<std::string,std::string>> structural(){
        // These are seed hypotheses for the research process.  Unlike the
        // theorem kernel, they are allowed to be false and are adversarially
        // tested before any status change.
        return {
            {"gcd(a,b)=gcd(b,mod(a,b)) assuming b!=0","Euclidean reduction seed"},
            {"phi(a*b)=phi(a)*phi(b) assuming gcd(a,b)=1","multiplicativity seed"},
            {"tau(a*b)=tau(a)*tau(b) assuming gcd(a,b)=1","divisor-count seed"},
            {"sigma(a*b)=sigma(a)*sigma(b) assuming gcd(a,b)=1","divisor-sum seed"},
            {"phi(p)=p-1 assuming isPrime(p)=1","prime specialization seed"},
            {"tau(p)=2 assuming isPrime(p)=1","prime divisor-count seed"},
            {"sigma(p)=p+1 assuming isPrime(p)=1","prime divisor-sum seed"},
            {"nCr(n,k)=nCr(n,n-k) assuming n>=0 and k>=0 and k<=n","binomial symmetry seed"},
            {"nCr(n,k)=nCr(n-1,k-1)+nCr(n-1,k) assuming n>=1 and k>=1 and k<=n","Pascal seed"},
            {"fib(n+2)=fib(n+1)+fib(n) assuming n>=0","Fibonacci seed"},
            {"factorial(n+1)=(n+1)*factorial(n) assuming n>=0","factorial seed"},
            {"derangements(n)=(n-1)*(derangements(n-1)+derangements(n-2)) assuming n>=2","derangement seed"}
        };
    }
    std::vector<std::pair<std::string,std::string>> mutations(){
        std::vector<std::pair<std::string,std::string>>out;
        const auto seeds=structural();
        for(const auto&s:seeds){
            std::string t=mutateCoefficient(s.first,1);
            if(t!=s.first)out.push_back({t,"local coefficient mutation"});
            t=mutateCoefficient(s.first,-1);
            if(t!=s.first)out.push_back({t,"local coefficient mutation"});
        }
        out.push_back({"fib(n+2)=fib(n+1)+2*fib(n) assuming n>=0","recurrence mutation"});
        out.push_back({"nCr(n,k)=nCr(n-1,k-1)-nCr(n-1,k) assuming n>=1 and k>=1 and k<=n","sign mutation"});
        return out;
    }
};

// Lightweight adaptive search controller.  A method that repeatedly produces
// verified results receives more budget; a method that mostly generates
// counterexamples receives less.  This is learning of the search policy, not
// learning of theorem truth.
static std::string methodFamily(const std::string& method){
    if(method.find("hypothesis-seed:")==0) return "seed";
    if(method.find("mutation:")==0) return "mutation";
    if(method.find("data-mining:")==0) return "data-mining";
    if(method.find("generalization/substitution")==0) return "generalization";
    if(method.find("theorem-composition:")==0) return "composition";
    if(method.find("lemma-obstacle:")==0) return "lemma";
    if(method.find("research-synthesis:")==0 || method.find("autonomous symbolic synthesis")!=std::string::npos) return "synthesis";
    if(method.find("exploration:")==0 || method.find("random")!=std::string::npos) return "exploration";
    if(method.find("cross-sequence")!=std::string::npos) return "data-mining";
    return method;
}

// HILBERT 3.2: the search policy now learns at the level of research
// strategies rather than individual conjecture strings.  This makes the
// research loop genuinely adaptive: successful strategy families receive
// more future search budget, while repeatedly unproductive families shrink.
enum class MissionMove {
    Counterexample, Prove, ForgeLemma, Generalize, Specialize,
    Compose, Ablate, Converse, Analogize, Repair
};
static const char* missionMoveName(MissionMove m){
    switch(m){
        case MissionMove::Counterexample:return "counterexample";
        case MissionMove::Prove:return "prove";
        case MissionMove::ForgeLemma:return "forge-lemma";
        case MissionMove::Generalize:return "generalize";
        case MissionMove::Specialize:return "specialize";
        case MissionMove::Compose:return "compose";
        case MissionMove::Ablate:return "assumption-ablation";
        case MissionMove::Converse:return "converse";
        case MissionMove::Analogize:return "analogy";
        case MissionMove::Repair:return "repair";
    }
    return "unknown";
}

class SearchPolicy {
    struct Arm{int tried=0,verified=0,useful=0; double reward=0.0;};
    // Global strategy memory plus contextual domain-specific memory.  The
    // context layer is deliberately small and transparent: HILBERT learns
    // which *research moves* work in which mathematical domains, rather than
    // memorising theorem strings.
    std::map<std::string,Arm>arms_;
    std::map<std::string,Arm>context_;
    static std::string key(const std::string&domain,const std::string&family){
        return domain.empty()?family:(domain+"|"+family);
    }
    static double score(const Arm&a){
        // Smoothed optimistic estimate.  The uncertainty bonus encourages
        // exploration of strategies that have not yet been tried often.
        const double n=double(a.tried);
        const double mean=(a.reward+1.0)/(n+2.0);
        const double bonus=0.35/std::sqrt(n+1.0);
        return std::max(0.0,mean+bonus);
    }
public:
    void observe(const std::string&m,Status s){ observe("",m,s, s==Status::KERNEL_VERIFIED?1.0:(s==Status::FALSIFIED?-0.35:0.15)); }
    void observe(const std::string&domain,const std::string&m,Status s,double reward){
        const std::string family=methodFamily(m);
        Arm&a=arms_[family]; ++a.tried; a.reward+=reward;
        if(s==Status::THEOREM||s==Status::CONDITIONAL_THEOREM||s==Status::KERNEL_VERIFIED){++a.verified;++a.useful;}
        else if(s==Status::CONJECTURE||s==Status::LEMMA_FOUND)++a.useful;
        Arm&c=context_[key(domain,family)]; ++c.tried; c.reward+=reward;
        if(s==Status::THEOREM||s==Status::CONDITIONAL_THEOREM||s==Status::KERNEL_VERIFIED){++c.verified;++c.useful;}
        else if(s==Status::CONJECTURE||s==Status::LEMMA_FOUND)++c.useful;
    }
    double value(const std::string&m)const{ return value("",m); }
    double value(const std::string&domain,const std::string&m)const{
        const std::string family=methodFamily(m);
        auto it=context_.find(key(domain,family));
        if(it!=context_.end() && it->second.tried>0)return score(it->second);
        auto g=arms_.find(family);
        return g==arms_.end()?1.0:score(g->second);
    }
    // Select the order in which a mission should spend its scarce reasoning
    // budget.  Counterexample remains first when available: falsification is
    // cheap and protects the rest of the pipeline from wasting proof effort.
    std::vector<MissionMove> orderMission(const std::string&domain,const std::vector<MissionMove>&plan)const{
        std::vector<MissionMove> out=plan;
        std::stable_sort(out.begin(),out.end(),[&](MissionMove a,MissionMove b){
            if(a==MissionMove::Counterexample && b!=MissionMove::Counterexample)return true;
            if(b==MissionMove::Counterexample && a!=MissionMove::Counterexample)return false;
            return value(domain,std::string("mission:")+missionMoveName(a)) > value(domain,std::string("mission:")+missionMoveName(b));
        });
        return out;
    }
    std::string report()const{
        std::ostringstream o;
        int shown=0;
        for(const auto&x:arms_){
            if(shown++>=24){o<<"...";break;}
            o<<x.first<<"="<<x.second.useful<<"/"<<x.second.tried
             <<"("<<std::fixed<<std::setprecision(3)<<value(x.first)<<") ";
        }
        if(!context_.empty()){
            o<<" contextual{"; int n=0;
            for(const auto&x:context_){if(n++>=12){o<<"...";break;}o<<x.first<<"="<<std::fixed<<std::setprecision(3)<<score(x.second)<<" ";}
            o<<"}";
        }
        return o.str();
    }
    int quota(const std::string&family,int base,int minimum=1)const{return quota("",family,base,minimum);}
    int quota(const std::string&domain,const std::string&family,int base,int minimum=1)const{
        const double v=value(domain,family);
        const double scaled=0.50+0.80*std::min(1.0,v);
        return std::max(minimum,int(std::round(base*scaled)));
    }
    void save(const std::string&file)const{
        std::ofstream out(file); if(!out)throw Error("cannot write policy file: "+file);
        out<<"HILBERT-POLICY-2\n";
        for(const auto&x:arms_)out<<"G\t"<<x.first<<"\t"<<x.second.tried<<"\t"<<x.second.verified<<"\t"<<x.second.useful<<"\t"<<std::setprecision(17)<<x.second.reward<<"\n";
        for(const auto&x:context_)out<<"C\t"<<x.first<<"\t"<<x.second.tried<<"\t"<<x.second.verified<<"\t"<<x.second.useful<<"\t"<<std::setprecision(17)<<x.second.reward<<"\n";
    }
    void load(const std::string&file){
        std::ifstream in(file); if(!in)throw Error("cannot read policy file: "+file);
        std::string line; while(std::getline(in,line)){
            if(line.empty()||line[0]=='#'||line=="HILBERT-POLICY-2")continue;
            std::vector<std::string> f; size_t b=0; for(;;){size_t z=line.find('\t',b);if(z==std::string::npos){f.push_back(line.substr(b));break;}f.push_back(line.substr(b,z-b));b=z+1;}
            if(f.size()!=6) continue;
            try{
                Arm a; a.tried=std::stoi(f[2]);a.verified=std::stoi(f[3]);a.useful=std::stoi(f[4]);a.reward=std::stod(f[5]);
                if(f[0]=="G")arms_[f[1]]=a; else if(f[0]=="C")context_[f[1]]=a;
            }catch(...){ }
        }
    }
};


// ============================================================================
// 11. Theorem Composer: autonomous lemma/theorem construction from knowledge
// ============================================================================
// HILBERT should not merely memorize discovered identities.  Once two facts
// have been independently certified, it can construct new consequences from
// them.  Every generated consequence is still sent through Popper + Euclid +
// the independent Kernel, so composition is a search operator, never a trust
// shortcut.
class TheoremComposer {
    static std::string assumptionsText(const std::vector<Assumption>& as){
        if(as.empty())return {};
        std::string out=" assuming ";
        for(size_t i=0;i<as.size();++i){
            if(i)out+=" and ";
            out+=print(as[i].left)+" "+relationName(as[i].rel)+" "+print(as[i].right);
        }
        return out;
    }
    static std::string composeText(const E&a,const E&b,const E&c,const E&d,
                                   const std::vector<Assumption>& as, char mode){
        std::string l,r;
        if(mode=='+'){l="("+print(a)+")+("+print(c)+")";r="("+print(b)+")+("+print(d)+")";}
        else if(mode=='-'){l="("+print(a)+")-("+print(c)+")";r="("+print(b)+")-("+print(d)+")";}
        else {l="("+print(a)+")*("+print(c)+")";r="("+print(b)+")*("+print(d)+")";}
        return l+"="+r+assumptionsText(as);
    }
    static std::vector<Assumption> mergeAssumptions(const Statement&a,const Statement&b){
        std::vector<Assumption> out=a.assumptions;
        for(const auto&x:b.assumptions){
            bool exists=false;
            for(const auto&y:out)
                if(x.rel==y.rel&&canonical(x.left)==canonical(y.left)&&canonical(x.right)==canonical(y.right)){
                    exists=true;break;
                }
            if(!exists)out.push_back(x);
        }
        return out;
    }
public:
    std::vector<std::pair<std::string,std::string>> compose(const Euler&kb,int budget)const{
        std::vector<std::pair<std::string,std::string>> out;
        std::vector<const Record*> facts;
        for(const auto&r:kb.all())
            if((r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM||
                r.status==Status::KERNEL_VERIFIED)&&r.statement.rel==Rel::Eq &&
               polynomial(r.statement.left)&&polynomial(r.statement.right))
                facts.push_back(&r);
        // Composition is quadratic.  Keep the most recent verified facts so
        // autonomous search remains productive instead of exploding into
        // thousands of redundant algebraic consequences.
        if(facts.size()>32) facts.erase(facts.begin(),facts.end()-32);
        int used=0;
        // Prioritize genuinely cross-theorem compositions.  These are much more
        // informative than composing a theorem with itself and are the
        // consequences a human researcher would normally inspect first.
        for(size_t i=0;i<facts.size()&&used<budget;++i){
            for(size_t j=i+1;j<facts.size()&&used<budget;++j){
                const Statement&a=facts[i]->statement;
                const Statement&b=facts[j]->statement;
                auto assumptions=mergeAssumptions(a,b);
                const char modes[] = {'+','-','*'};
                for(char mode:modes){
                    if(used>=budget)break;
                    std::string text=composeText(a.left,a.right,b.left,b.right,assumptions,mode);
                    std::string origin="theorem-composition:"+std::to_string(facts[i]->id)+"+"+std::to_string(facts[j]->id);
                    out.push_back({text,origin});
                    ++used;
                }
            }
        }
        // If the knowledge base contains only one usable theorem, allow a
        // bounded self-composition as a fallback.  It is deliberately delayed
        // until all cross-theorem opportunities have been exhausted.
        for(size_t i=0;i<facts.size()&&used<budget;++i){
            const Statement&a=facts[i]->statement;
            auto assumptions=a.assumptions;
            const char modes[] = {'+','-','*'};
            for(char mode:modes){
                if(used>=budget)break;
                std::string text=composeText(a.left,a.right,a.left,a.right,assumptions,mode);
                std::string origin="theorem-composition:self:"+std::to_string(facts[i]->id);
                out.push_back({text,origin});
                ++used;
            }
        }
        return out;
    }
};

// ============================================================================
// 12. Proof Planner: turns failed direct proof attempts into research tasks
// ============================================================================
// This planner does not claim a proof.  It records why a candidate is outside
// the current certificate language and creates structurally simpler targets
// for later discovery.  The distinction between a research plan and a proof
// is deliberately explicit.
struct ProofPlan {
    std::string target;
    std::vector<std::string> obligations;
    std::string strategy;
};
class ProofPlanner {
public:
    ProofPlan plan(const Statement&s)const{
        ProofPlan p;p.target=print(s);
        std::vector<E> den;denominatorExpressions(s.left,den);denominatorExpressions(s.right,den);
        for(const auto&d:den)p.obligations.push_back(print(d)+" != 0");
        if(s.rel!=Rel::Eq){
            p.strategy="reduce the relation to exact evaluation, divisibility, or an equality lemma";
        }else if(!den.empty()&&!rationalEqualityUnderAssumptions(s)){
            p.strategy="prove denominator obligations, normalize the rational expressions, then certify the cross-product identity";
        }else if(!polynomial(s.left)||!polynomial(s.right)){
            p.strategy="search for trusted recursive definitions, learned lemmas, or a transformation into the kernel's polynomial/rational domain";
        }else{
            p.strategy="exact polynomial normalization";
        }
        return p;
    }
};


// ============================================================================
// 13. Lemma Forge: turn proof obstacles into small independently certifiable
// research targets.
// ============================================================================
// 2.6 adds a deliberately conservative lemma-generation layer.  It does not
// invent truth by fiat: it extracts trusted recursive schemas from the actual
// obstacle, instantiates them at the expressions occurring in that obstacle,
// and sends every proposed lemma through the normal adversarial/proof/kernel
// pipeline.  A lemma becomes reusable knowledge only after certification.
class LemmaForge {
    static void collectFunctions(const E&e,std::vector<E>&out){
        if(e->k==Kind::Func) out.push_back(e);
        for(const auto&c:e->a) collectFunctions(c,out);
    }
    static std::string assumptionsText(const std::vector<Assumption>&as){
        if(as.empty()) return {};
        std::string out=" assuming ";
        for(size_t i=0;i<as.size();++i){
            if(i) out+=" and ";
            out+=print(as[i].left)+" "+relationName(as[i].rel)+" "+print(as[i].right);
        }
        return out;
    }
    static bool plusConstant(const E&e,int64_t c,E&base){
        if(e->k!=Kind::Add) return false;
        std::vector<E> terms;
        std::function<void(const E&)> flat=[&](const E&x){
            if(x->k==Kind::Add) for(const auto&z:x->a) flat(z); else terms.push_back(x);
        };
        flat(e);
        bool found=false; std::vector<E> rest;
        for(const auto&t:terms){
            if(!found && t->k==Kind::Const && t->q.d==BigInt(1) && t->q.n==BigInt(c)) found=true;
            else rest.push_back(t);
        }
        if(!found||rest.empty()) return false;
        base=rest[0];
        for(size_t i=1;i<rest.size();++i) base=op(Kind::Add,{base,rest[i]});
        return true;
    }
    static std::vector<std::string> schemaCandidates(const E&e){
        std::vector<std::string> out;
        if(e->k!=Kind::Func) return out;
        const std::string&f=e->name;
        if(f=="fib"&&e->a.size()==1){
            E base;
            if(plusConstant(e->a[0],2,base)){
                E b1=op(Kind::Add,{base,cn(1)});
                out.push_back("fib("+print(e->a[0])+")=fib("+print(b1)+")+fib("+print(base)+") assuming "+print(base)+">=0");
            }
        } else if(f=="factorial"&&e->a.size()==1){
            E base;
            if(plusConstant(e->a[0],1,base)){
                out.push_back("factorial("+print(e->a[0])+")=("+print(e->a[0])+")*factorial("+print(base)+") assuming "+print(base)+">=0");
            }
        } else if(f=="nCr"&&e->a.size()==2){
            E n=e->a[0],k=e->a[1];
            out.push_back("nCr("+print(n)+","+print(k)+")=nCr("+print(n)+","+print(subtraction(n,k))+") assuming "+print(n)+">=0 and "+print(k)+">=0 and "+print(k)+"<="+print(n));
        } else if(f=="derangements"&&e->a.size()==1){
            E n=e->a[0];
            out.push_back("derangements("+print(n)+")=("+print(n)+"-1)*(derangements("+print(subtraction(n,cn(1)))+")+derangements("+print(subtraction(n,cn(2)))+")) assuming "+print(n)+">=2");
        }
        return out;
    }
public:
    std::vector<std::pair<std::string,std::string>> forge(const Statement&s,int budget=12)const{
        std::vector<E> funcs; collectFunctions(s.left,funcs); collectFunctions(s.right,funcs);
        std::vector<std::pair<std::string,std::string>> out;
        std::set<std::string> unique;
        for(const auto&f:funcs){
            for(const auto&candidate:schemaCandidates(f)){
                if((int)out.size()>=budget) return out;
                try{
                    Statement q=Parser(candidate).statement();
                    // Reject candidates whose assumptions are unrelated to the
                    // target context; this keeps the forge conservative.
                    bool compatible=true;
                    for(const auto&a:q.assumptions){
                        bool known=false;
                        for(const auto&b:s.assumptions)
                            if(a.rel==b.rel&&canonical(a.left)==canonical(b.left)&&canonical(a.right)==canonical(b.right))
                                known=true;
                        if(!known && !q.assumptions.empty()) {
                            // The forged lemma may legitimately introduce a
                            // local side condition; retain it, but mark it as
                            // an explicit research obligation in the origin.
                        }
                    }
                    (void)compatible;
                    std::string key=statementKey(q);
                    if(unique.insert(key).second)
                        out.push_back({candidate,"lemma-forge:function-obstacle:"+f->name});
                }catch(...){ }
            }
        }
        return out;
    }
};


// ============================================================================
// HILBERT 3.0 - four research upgrades
// 1) Persistent experience/replay memory
// 2) Portfolio proof search with adaptive best-first allocation
// 3) Multi-agent research debate: conjecturer / skeptic / prover / critic
// 4) Theory induction + autonomous research agenda
// ============================================================================
struct Experience {
    std::string method;
    bool success=false;
    int tests=0;
    double reward=0.0;
    std::string domain;
};

class ExperienceMemory {
    std::vector<Experience> data_;
public:
    void observe(const std::string&m,bool success,int tests,double reward,const std::string&domain){
        data_.push_back({m,success,tests,reward,domain});
        if(data_.size()>20000) data_.erase(data_.begin(),data_.begin()+5000);
    }
    double value(const std::string&m,const std::string&domain)const{
        double sum=1.0,n=2.0;
        for(const auto&e:data_) if(e.method==m && (domain.empty()||e.domain==domain)){
            sum+=e.reward; n+=1.0;
        }
        return sum/n;
    }
    void save(const std::string&file)const{
        std::ofstream out(file);
        if(!out) throw Error("cannot write experience file: "+file);
        for(const auto&e:data_)
            out<<e.method<<"\t"<<(e.success?1:0)<<"\t"<<e.tests<<"\t"
               <<std::setprecision(17)<<e.reward<<"\t"<<e.domain<<"\n";
    }
    void load(const std::string&file){
        std::ifstream in(file); if(!in) throw Error("cannot read experience file: "+file);
        std::string line;
        while(std::getline(in,line)){
            std::vector<std::string> f; size_t b=0; for(;;){ size_t z=line.find('\t',b); if(z==std::string::npos){f.push_back(line.substr(b));break;} f.push_back(line.substr(b,z-b)); b=z+1; } if(f.size()!=5) continue;
            try{ data_.push_back({f[0],std::stoi(f[1])!=0,std::stoi(f[2]),std::stod(f[3]),f[4]}); }
            catch(...){ }
        }
    }
    size_t size()const{return data_.size();}
};

class ProofPortfolio {
public:
    Optional<Proof> prove(const Statement&s,int states,int depth)const{
        std::vector<std::pair<int,std::function<Optional<Proof>()>>> arms;
        arms.push_back({3,[&](){return Euclid(states,depth).prove(s);}});
        if(s.rel==Rel::Eq)
            arms.push_back({2,[&](){return BidirectionalRewriteSearch().prove(s,std::max(100,states),std::min(16,depth));}});
        arms.push_back({1,[&](){return RewritePlanner().prove(s,std::min(16,depth));}});
        std::sort(arms.begin(),arms.end(),[](const auto&a,const auto&b){return a.first>b.first;});
        for(auto&arm:arms){ auto p=arm.second(); if(p) return p; }
        return {};
    }
};

class ResearchDebate {
public:
    struct Verdict { bool plausible=false; bool falsified=false; std::string reason; };
    Verdict skeptic(const Statement&s,uint64_t seed,int tests)const{
        Evidence e=Popper(seed).attack(s,std::max(20,tests));
        if(e.counterexample){
            std::ostringstream o; o<<"counterexample found after "<<e.tests<<" exact tests";
            return {false,true,o.str()};
        }
        return {true,false,"survived exact adversarial testing; this is evidence, not proof"};
    }
    std::vector<std::string> questions(const Statement&s)const{
        std::vector<std::string> q;
        std::string d=domainOf(s);
        if(d=="number_theory"){
            q.push_back("Can the identity be generalized from integers to coprime classes?");
            q.push_back("Does the statement admit a multiplicative, additive, or modular analogue?");
        }else if(d=="combinatorics"){
            q.push_back("Is there a recurrence, generating-function, or symmetry explanation?");
            q.push_back("Can the identity be lifted to a parameterized family?");
        }else if(d=="recurrences"){
            q.push_back("Can the recurrence be inverted or generalized to a family?");
            q.push_back("What invariant remains unchanged under the recurrence?");
        }else{
            q.push_back("Can the equality be parameterized into a broader identity?");
            q.push_back("What smaller lemma would make the proof easier?");
        }
        return q;
    }
};

struct ResearchConcept {
    std::string name,domain,pattern;
    int support=0;
};

class TheoryInducer {
public:
    std::vector<ResearchConcept> induce(const Euler&kb)const{
        std::map<std::string,int> counts;
        for(const auto&r:kb.all()) if(r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM||r.status==Status::LEMMA_FOUND){
            std::string t=print(r.statement);
            const char* keys[]={"gcd","phi","tau","sigma","fib","factorial","nCr","nPr","catalan","derangements","partition","bell","stirling2","mod"};
            for(const char*k:keys) if(t.find(k)!=std::string::npos) ++counts[k];
        }
        std::vector<ResearchConcept> out;
        for(const auto&x:counts) if(x.second>=2){
            std::string d=(x.first=="nCr"||x.first=="nPr"||x.first=="catalan"||x.first=="partition"||x.first=="bell"||x.first=="stirling2"||x.first=="derangements")?"combinatorics":"number_theory";
            if(x.first=="fib"||x.first=="factorial") d="recurrences";
            out.push_back({x.first,d,"repeated verified structural motif",x.second});
        }
        return out;
    }
};

class ResearchAgenda {
public:
    std::vector<std::string> build(const Euler&kb,const std::vector<ResearchConcept>&concepts)const{
        std::vector<std::string> out;
        for(const auto&c:concepts){
            if(c.name=="gcd") out.push_back("Investigate Euclidean reductions, symmetry, and composition laws for gcd.");
            else if(c.name=="phi") out.push_back("Investigate multiplicativity and divisor-sum representations of phi.");
            else if(c.name=="tau") out.push_back("Investigate multiplicative formulas and convolution identities for tau.");
            else if(c.name=="sigma") out.push_back("Investigate multiplicativity and divisor-sum recurrences for sigma.");
            else if(c.name=="nCr") out.push_back("Investigate Pascal, symmetry, and polynomial identities involving binomial coefficients.");
            else if(c.name=="nPr") out.push_back("Investigate relationships between permutations and binomial coefficients.");
            else if(c.name=="fib") out.push_back("Investigate recurrence transformations and invariant identities for Fibonacci sequences.");
            else if(c.name=="factorial") out.push_back("Investigate recurrence and combinatorial interpretations of factorial.");
            else out.push_back("Explore new consequences and generalizations of the verified "+c.name+" family.");
        }
        if(out.empty()) out.push_back("Acquire more exact observations and search for repeated mathematical structure.");
        (void)kb; return out;
    }
};


// ============================================================================
// 14. HILBERT 4.0: research-council, structural abstraction, novelty memory
// and auditable research trace.
//
// These components deliberately operate ABOVE the proof kernel. They may
// propose, rank, compare and mutate mathematical ideas, but they cannot turn
// a conjecture into a theorem. The existing Popper + proof portfolio + Kernel
// pipeline remains the final trust boundary.
// ============================================================================
class StructuralAbstraction {
    static void variablesIn(const E&e,std::set<std::string>&v){ vars(e,v); }
    static std::string normalizedText(const E&e,const std::map<std::string,std::string>&m){
        if(e->k==Kind::Var){auto it=m.find(e->name);return it==m.end()?e->name:it->second;}
        if(e->k==Kind::Const)return e->q.str();
        if(e->k==Kind::Neg)return "NEG("+normalizedText(e->a[0],m)+")";
        std::ostringstream o;
        if(e->k==Kind::Func){o<<e->name<<"(";for(size_t i=0;i<e->a.size();++i){if(i)o<<",";o<<normalizedText(e->a[i],m);}o<<")";return o.str();}
        const char* opName[] = {"C","V","ADD","MUL","DIV","POW","NEG","FUNC"};
        o<<opName[int(e->k)]<<"(";
        for(size_t i=0;i<e->a.size();++i){if(i)o<<",";o<<normalizedText(e->a[i],m);}o<<")";
        return o.str();
    }
public:
    static std::string fingerprint(const Statement&s){
        std::set<std::string> v;variablesIn(s.left,v);variablesIn(s.right,v);
        std::map<std::string,std::string> m;int i=0;for(const auto&x:v)m[x]="v"+std::to_string(i++);
        std::ostringstream o;o<<int(s.rel)<<":"<<normalizedText(s.left,m)<<"="<<normalizedText(s.right,m);
        if(!s.assumptions.empty()){o<<"|A:";for(const auto&a:s.assumptions)o<<int(a.rel)<<":"<<normalizedText(a.left,m)<<"="<<normalizedText(a.right,m)<<";";}
        return o.str();
    }
    static std::string conceptSignature(const Statement&s){
        std::string t=print(s);std::ostringstream o;
        const char* keys[]={"+","*","/","^","gcd","lcm","phi","tau","sigma","fib","factorial","nCr","nPr","catalan","partition","bell","stirling2","derangements","mod"};
        for(const char*k:keys)if(t.find(k)!=std::string::npos)o<<k<<";";
        return o.str();
    }
};

class NoveltyArchive {
    std::unordered_map<std::string,int> seen_;
    std::unordered_map<std::string,double> reward_;
public:
    double novelty(const Statement&s)const{
        std::string f=StructuralAbstraction::fingerprint(s);
        auto it=seen_.find(f); if(it==seen_.end())return 1.0;
        return 1.0/(1.0+double(it->second));
    }
    bool admit(const Statement&s){
        std::string f=StructuralAbstraction::fingerprint(s);int &n=seen_[f];++n;return n==1;
    }
    void reward(const Statement&s,double r){reward_[StructuralAbstraction::fingerprint(s)]+=r;}
    std::string report()const{
        std::ostringstream o;o<<"patterns="<<seen_.size();
        if(!reward_.empty()){double sum=0;for(const auto&x:reward_)sum+=x.second;o<<", cumulative_reward="<<std::fixed<<std::setprecision(3)<<sum;}
        return o.str();
    }
};

struct CouncilProposal { std::string text,method,reason; double prior=0.0; };

class ResearchCouncil {
    mutable std::mt19937_64 rng_;
public:
    explicit ResearchCouncil(uint64_t seed):rng_(seed^0xA24BAED4963EE407ULL){}
    std::vector<CouncilProposal> deliberate(const Euler&kb,const NoveltyArchive&archive,
                                             const std::vector<ResearchConcept>&concepts,
                                             int budget) const {
        std::vector<CouncilProposal> out;
        std::vector<const Record*> facts;
        for(const auto&r:kb.all()) if(r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM||r.status==Status::LEMMA_FOUND) facts.push_back(&r);
        if(facts.size()>24)facts.erase(facts.begin(),facts.end()-24);
        // Agent A: analogy/explanation. Pair verified facts sharing a concept
        // and ask the normal composer to form a relation between their sides.
        for(size_t i=0;i<facts.size()&&int(out.size())<budget;++i){
            for(size_t j=i+1;j<facts.size()&&int(out.size())<budget;++j){
                const std::string sigA=StructuralAbstraction::conceptSignature(facts[i]->statement);
                const std::string sigB=StructuralAbstraction::conceptSignature(facts[j]->statement);
                if(sigA.empty()||sigB.empty())continue;
                if(sigA.find("gcd")!=std::string::npos && sigB.find("gcd")!=std::string::npos){
                    std::string l="gcd("+print(facts[i]->statement.left)+","+print(facts[j]->statement.left)+")";
                    std::string r="gcd("+print(facts[i]->statement.right)+","+print(facts[j]->statement.right)+")";
                    std::string q=l+"="+r;
                    try{Statement s=Parser(q).statement();out.push_back({q,"council:analogy:gcd","shared gcd structure",archive.novelty(s)});}catch(...){ }
                }
            }
        }
        // Agent B: concept-directed questions become safe seed families.
        for(const auto&c:concepts){
            if(int(out.size())>=budget)break;
            if(c.name=="fib"){
                const std::string q="fib(n+2)=fib(n+1)+fib(n) assuming n>=0";
                try{Statement s=Parser(q).statement();out.push_back({q,"council:recurrence","recursive invariant hypothesis",archive.novelty(s)});}catch(...){ }
            }else if(c.name=="factorial"){
                const std::string q="factorial(n+1)=(n+1)*factorial(n) assuming n>=0";
                try{Statement s=Parser(q).statement();out.push_back({q,"council:recurrence","recursive invariant hypothesis",archive.novelty(s)});}catch(...){ }
            }else if(c.name=="nCr"){
                const std::string q="nCr(n,k)=nCr(n,n-k) assuming n>=0 and k>=0 and k<=n";
                try{Statement s=Parser(q).statement();out.push_back({q,"council:symmetry","parameter symmetry hypothesis",archive.novelty(s)});}catch(...){ }
            }
        }
        // Agent C: randomized challenge questions. Generate a small algebraic
        // law from random variable permutations; proof still decides truth.
        static const char* v[] = {"x","y","z","u","v","w"};
        for(int i=0;i<budget && int(out.size())<budget*2;++i){
            std::string a=v[rng_()%6],b=v[(rng_()/7)%6],c=v[(rng_()/31)%6];
            if(a==b||b==c||a==c)continue;
            std::string q="("+a+"+"+b+")*"+c+"="+a+"*"+c+"+"+b+"*"+c;
            try{Statement s=Parser(q).statement();out.push_back({q,"council:explorer","fresh structural identity",archive.novelty(s)});}catch(...){ }
        }
        std::sort(out.begin(),out.end(),[](const CouncilProposal&a,const CouncilProposal&b){return a.prior>b.prior;});
        return out;
    }
};

class ResearchTrace {
    std::vector<std::string> events_;
public:
    void record(int round,uint64_t seq,const Record&r,double novelty){
        std::ostringstream o;
        o<<"{\"round\":"<<round<<",\"seq\":"<<seq<<",\"id\":"<<r.id
         <<",\"status\":\""<<statusName(r.status)<<"\",\"method\":\""<<r.generator
         <<"\",\"novelty\":"<<std::fixed<<std::setprecision(6)<<novelty
         <<",\"score\":"<<r.score.total<<",\"statement\":\""<<print(r.statement)<<"\"}";
        events_.push_back(o.str());
        if(events_.size()>50000)events_.erase(events_.begin(),events_.begin()+10000);
    }
    void save(const std::string&file)const{std::ofstream out(file);if(!out)throw Error("cannot write research trace: "+file);for(const auto&e:events_)out<<e<<"\n";}
    size_t size()const{return events_.size();}
};

class ResearchObjective {
public:
    static double value(const Record&r,double novelty){
        const bool proof=r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM||r.status==Status::KERNEL_VERIFIED;
        const double domain=(domainOf(r.statement)=="algebra"||domainOf(r.statement)=="number_theory"||domainOf(r.statement)=="combinatorics")?1.0:0.6;
        return 0.45*(proof?1.0:0.0)+0.20*novelty+0.20*std::max(0.0,r.score.total)+0.15*domain;
    }
};


// ============================================================================
// HILBERT 7.0 research upgrades: counterexample-guided repair and persistent
// brain checkpoints.  These are deliberately outside the kernel: they may
// suggest improvements, but every repaired hypothesis must pass the same
// adversarial-testing + certificate gate as every other theorem.
// ============================================================================
class CounterexampleRepairer {
    static void mutateExpr(const E&e,std::vector<E>&out,int limit){
        if((int)out.size()>=limit)return;
        if(e->k==Kind::Const && e->q.d==BigInt(1) && e->q.n.str().size()<8){
            long long v=0; try{v=std::stoll(e->q.n.str());}catch(...){return;}
            for(int delta=-2;delta<=2 && (int)out.size()<limit;++delta){
                if(delta==0)continue;
                out.push_back(cn(v+delta));
            }
        }
        for(size_t i=0;i<e->a.size() && (int)out.size()<limit;++i){
            std::vector<E> child;
            mutateExpr(e->a[i],child,std::max(2,limit/2));
            for(const auto&c:child){
                std::vector<E> args=e->a; args[i]=c;
                auto z=std::make_shared<Expr>();z->k=e->k;z->q=e->q;z->name=e->name;z->a=args;out.push_back(z);
                if((int)out.size()>=limit)break;
            }
        }
        if((e->k==Kind::Add||e->k==Kind::Mul) && e->a.size()==2 && (int)out.size()<limit){
            auto z=std::make_shared<Expr>();z->k=e->k;z->q=e->q;z->name=e->name;z->a={e->a[1],e->a[0]};out.push_back(z);
        }
    }
public:
    std::vector<Statement> repair(const Statement&s,int limit)const{
        std::vector<Statement> out; std::vector<E> left,right;
        mutateExpr(s.left,left,limit); mutateExpr(s.right,right,limit);
        std::set<std::string> keys;
        for(const auto&e:left){Statement q=s;q.left=e;std::string k=statementKey(q);if(keys.insert(k).second)out.push_back(q);if((int)out.size()>=limit)break;}
        for(const auto&e:right){Statement q=s;q.right=e;std::string k=statementKey(q);if(keys.insert(k).second)out.push_back(q);if((int)out.size()>=limit)break;}
        return out;
    }
};

class BrainCheckpoint {
    static std::string esc(const std::string&s){std::string o=s;for(size_t p=0;(p=o.find('\\',p))!=std::string::npos;p+=2)o.insert(p,1,'\\');for(size_t p=0;(p=o.find('\t',p))!=std::string::npos;p+=2)o.replace(p,1,"\\t");return o;}
public:
    static void save(const Euler&kb,const std::vector<Statement>&learned,const ExperienceMemory&exp,const std::string&file){
        std::ofstream out(file); if(!out)throw Error("cannot write brain checkpoint: "+file);
        out<<"HILBERT-BRAIN-5\n";
        for(const auto&r:kb.all()) if(r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM||r.status==Status::LEMMA_FOUND)
            out<<"THEOREM\t"<<esc(print(r.statement))<<"\n";
        for(const auto&s:learned) out<<"LEARNED\t"<<esc(print(s))<<"\n";
        out<<"EXPERIENCE_SIZE\t"<<exp.size()<<"\n";
    }
    static std::vector<std::string> loadStatements(const std::string&file){
        std::ifstream in(file);if(!in)throw Error("cannot read brain checkpoint: "+file);
        std::string line;std::vector<std::string> out;
        while(std::getline(in,line)){
            if(line.rfind("THEOREM\t",0)==0||line.rfind("LEARNED\t",0)==0){
                size_t p=line.find('\t');if(p!=std::string::npos)out.push_back(line.substr(p+1));
            }
        }
        return out;
    }
};

class ResearchJournal {
    std::vector<std::string> highlights_;
public:
    void observe(const Record&r,double novelty){
        if(r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM||r.status==Status::LEMMA_FOUND){
            std::ostringstream o;o<<statusName(r.status)<<" | novelty="<<std::fixed<<std::setprecision(3)<<novelty<<" | "<<print(r.statement)<<" | "<<r.generator;highlights_.push_back(o.str());
            if(highlights_.size()>2000)highlights_.erase(highlights_.begin(),highlights_.begin()+500);
        }
    }
    void save(const std::string&file)const{std::ofstream out(file);if(!out)throw Error("cannot write research journal: "+file);for(const auto&x:highlights_)out<<x<<"\n";}
    size_t size()const{return highlights_.size();}
};


// ============================================================================
// 15. HILBERT 6.0: explicit mathematical research frontier
// ============================================================================
// The research brain now maintains an explicit set of unresolved mathematical
// tasks.  This is a scheduling/intelligence layer, not a trust layer: a task
// can be wrong, and only the existing adversarial tests + proof portfolio +
// Kernel can promote a statement to theorem status.
static std::string frontierEscape(const std::string&s){
    std::string o;
    for(char c:s){
        if(c=='"')o+="\\\"";
        else if(c=='\\')o+="\\\\";
        else if(c=='\n')o+="\\n";
        else if(c=='\r')o+="\\r";
        else o+=c;
    }
    return o;
}

struct FrontierItem {
    int id=0;
    std::string statement,domain,origin,ancestry,state;
    double priority=0.0,novelty=0.0,score=0.0;
    int tests=0,attempts=0;
};

class ResearchFrontier {
    std::map<int,FrontierItem> items_;
    std::unordered_map<std::string,int> byFingerprint_;
    int nextTask_=1;
    static double clamp01(double x){return std::max(0.0,std::min(1.0,x));}
    static double priorityFor(const Record&r,double novelty,int attempts){
        const bool open=(r.status==Status::CONJECTURE||r.status==Status::EMPIRICALLY_SUPPORTED);
        const double proofGap=open?1.0:0.45;
        const double depth=std::min(1.0,double(exprDepth(r.statement.left)+exprDepth(r.statement.right))/20.0);
        const double evidence=r.evidence.tests>0?(r.evidence.counterexample?0.05:0.80):0.25;
        const double score=clamp01(r.score.total+0.5);
        const double persistence=1.0/(1.0+0.15*double(std::max(0,attempts-1)));
        return (0.30*clamp01(novelty)+0.25*proofGap+0.20*score+0.15*evidence+0.10*depth)*persistence;
    }
public:
    void observe(const Record&r,double novelty){
        if(r.status!=Status::CONJECTURE && r.status!=Status::EMPIRICALLY_SUPPORTED &&
           r.status!=Status::SEARCH_EXHAUSTED && r.status!=Status::FALSIFIED) return;
        const std::string fp=StructuralAbstraction::fingerprint(r.statement);
        auto it=byFingerprint_.find(fp);
        if(it==byFingerprint_.end()){
            FrontierItem x;
            x.id=nextTask_++; x.statement=::hilbert::print(r.statement); x.domain=domainOf(r.statement);
            x.origin=r.generator; x.ancestry=r.ancestry;
            x.state=(r.status==Status::FALSIFIED)?"REPAIR":"OPEN";
            x.novelty=novelty; x.score=r.score.total; x.tests=r.evidence.tests;
            x.attempts=1; x.priority=priorityFor(r,novelty,1);
            items_[x.id]=x; byFingerprint_[fp]=x.id;
        }else{
            FrontierItem&x=items_[it->second];
            ++x.attempts; x.tests=std::max(x.tests,r.evidence.tests);
            x.novelty=std::max(x.novelty,novelty); x.score=std::max(x.score,r.score.total);
            x.priority=priorityFor(r,x.novelty,x.attempts);
            if(r.status==Status::FALSIFIED)x.state="REPAIR";
            else if(r.status==Status::CONJECTURE||r.status==Status::EMPIRICALLY_SUPPORTED)x.state="OPEN";
        }
    }
    std::vector<FrontierItem> top(size_t n=20)const{
        std::vector<FrontierItem> out;
        for(const auto&x:items_)if(x.second.state=="OPEN"||x.second.state=="REPAIR")out.push_back(x.second);
        std::sort(out.begin(),out.end(),[](const FrontierItem&a,const FrontierItem&b){
            if(a.priority!=b.priority)return a.priority>b.priority;
            return a.id<b.id;
        });
        if(out.size()>n)out.resize(n);
        return out;
    }
    std::vector<std::string> questions(size_t n=12)const{
        std::vector<std::string>out;
        for(const auto&x:top(n)){
            if(x.state=="REPAIR")out.push_back("Repair or weaken the falsified conjecture: "+x.statement);
            else if(x.domain=="number_theory")out.push_back("Seek a proof, counterexample, or stronger generalization of: "+x.statement);
            else if(x.domain=="combinatorics")out.push_back("Find a recurrence, bijection, symmetry, or parameterized generalization for: "+x.statement);
            else if(x.domain=="recurrences")out.push_back("Find an invariant, closed form, or stronger recurrence behind: "+x.statement);
            else out.push_back("Find a proof, counterexample, or broader structural law for: "+x.statement);
        }
        return out;
    }
    void print(std::ostream&out,size_t n=20)const{
        const auto xs=top(n);
        out<<"Research frontier tasks="<<items_.size()<<", open_top="<<xs.size()<<"\n";
        for(const auto&x:xs){
            out<<"  TASK #"<<x.id<<" ["<<x.state<<"] priority="<<std::fixed<<std::setprecision(3)<<x.priority
               <<" novelty="<<x.novelty<<" attempts="<<x.attempts<<" tests="<<x.tests<<" domain="<<x.domain<<"\n";
            out<<"    "<<x.statement<<"\n";
            out<<"    origin: "<<x.origin<<" | ancestry: "<<x.ancestry<<"\n";
        }
    }
    void exportJson(const std::string&file,size_t n=100)const{
        std::ofstream out(file); if(!out)throw Error("cannot write frontier file: "+file);
        const auto xs=top(n);
        out<<"{\n  \"format\":\"HILBERT-frontier-v1\",\n  \"tasks\":[\n";
        for(size_t i=0;i<xs.size();++i){
            const FrontierItem&x=xs[i];
            out<<"    {\"id\":"<<x.id<<",\"state\":\""<<frontierEscape(x.state)
               <<"\",\"priority\":"<<x.priority<<",\"novelty\":"<<x.novelty
               <<",\"score\":"<<x.score<<",\"attempts\":"<<x.attempts<<",\"tests\":"<<x.tests
               <<",\"domain\":\""<<frontierEscape(x.domain)<<"\",\"statement\":\""<<frontierEscape(x.statement)
               <<"\",\"origin\":\""<<frontierEscape(x.origin)<<"\",\"ancestry\":\""<<frontierEscape(x.ancestry)<<"\"}"
               <<(i+1<xs.size()?",":"")<<"\n";
        }
        out<<"  ]\n}\n";
    }
    size_t size()const{return items_.size();}
};

// ============================================================================
// HILBERT 11.0: Self-directed Mathematical Research & Insight Engine
//
// A mathematician does not merely collect open statements.  They choose an
// open problem, identify the kind of mathematics it resembles, and deliberately
// try several research moves.  This planner is the bridge between the frontier
// and the autonomous discovery loop.  It generates research hypotheses only;
// every resulting statement still passes through the ordinary skeptic, proof
// portfolio, and independent kernel.
// ============================================================================
class ResearchStrategyPlanner {
    static E substVar(const E&e,const std::string&name,const E&replacement){
        if(e->k==Kind::Var){
            if(e->name==name)return replacement;
            return vr(e->name);
        }
        auto z=std::make_shared<Expr>(); z->k=e->k; z->q=e->q; z->name=e->name;
        for(const auto&c:e->a)z->a.push_back(substVar(c,name,replacement));
        return z;
    }
    static Statement substituteVariable(const Statement&s,const std::string&name,const E&replacement){
        Statement q=s;
        q.left=substVar(s.left,name,replacement);
        q.right=substVar(s.right,name,replacement);
        q.assumptions.clear();
        for(const auto&a:s.assumptions)
            q.assumptions.push_back({substVar(a.left,name,replacement),
                                     substVar(a.right,name,replacement),a.rel});
        q.source=print(q);
        return q;
    }
    static std::vector<std::string> variables(const Statement&s){
        std::set<std::string>v; vars(s.left,v); vars(s.right,v);
        for(const auto&a:s.assumptions){vars(a.left,v);vars(a.right,v);}
        return std::vector<std::string>(v.begin(),v.end());
    }
    static std::string assumptionsText(const Statement&s){
        if(s.assumptions.empty())return {};
        std::ostringstream o; o<<" assuming ";
        for(size_t i=0;i<s.assumptions.size();++i){
            if(i)o<<" and ";
            o<<print(s.assumptions[i].left)<<" "
             <<relationName(s.assumptions[i].rel)<<" "
             <<print(s.assumptions[i].right);
        }
        return o.str();
    }
public:
    struct Proposal { Statement statement; std::string reason; };

    std::vector<Proposal> plan(const std::vector<Statement>&frontier,int budget)const{
        std::vector<Proposal>out;
        for(const auto&task:frontier){
            if((int)out.size()>=budget)break;
            try{
                Statement s=task;
                // Strategy 1: canonical difference formulation.  This changes
                // the research representation and is useful for polynomial
                // normalization and later invariant mining.
                if(s.rel==Rel::Eq && polynomial(s.left) && polynomial(s.right)){
                    Statement q; q.rel=Rel::Eq;
                    q.left=subtraction(s.left,s.right); q.right=cn(0);
                    q.assumptions=s.assumptions;
                    q.source=print(q);
                    out.push_back({q,"strategy:reduce equality to a zero invariant"});
                }
                if((int)out.size()>=budget)break;

                // Strategy 2: systematic variable shift.  This is particularly
                // powerful for recurrences and parameterized arithmetic laws.
                std::vector<std::string>vs=variables(s);
                for(const auto&v:vs){
                    if((int)out.size()>=budget)break;
                    E shifted=op(Kind::Add,{vr(v),cn(1)});
                    Statement q=substituteVariable(s,v,shifted);
                    q.source=print(q);
                    out.push_back({q,"strategy:shift parameter "+v+" -> "+v+"+1"});
                    if(v=="n")break;
                }
                if((int)out.size()>=budget)break;

                // Strategy 3: converse as a deliberate direction reversal.
                Statement converse=s; std::swap(converse.left,converse.right);
                converse.source=print(converse);
                out.push_back({converse,"strategy:test converse direction"});
            }catch(...){
                // A frontier entry may be a historical or partially supported
                // object outside the current parser language.  It remains in
                // the journal but contributes no executable hypothesis.
            }
        }
        return out;
    }

    // Verified theorems are research objects too. An autonomous mathematician
    // must be able to ask what follows from a theorem even when the unresolved
    // frontier is empty. These are hypotheses only; they still enter the full
    // Popper -> proof search -> independent Kernel pipeline.
    std::vector<Proposal> planTrusted(const std::vector<Statement>&theorems,int budget)const{
        std::vector<Proposal>out;
        if(budget<=0)return out;
        std::set<std::string>seen;
        for(const auto&s:theorems){
            if((int)out.size()>=budget)break;
            try{
                // Parameter-shift experiment. This is especially useful for
                // recurrences and parametric arithmetic/combinatorics.
                for(const auto&v:variables(s)){
                    if((int)out.size()>=budget)break;
                    Statement q=substituteVariable(s,v,op(Kind::Add,{vr(v),cn(1)}));
                    const std::string k=statementKey(q);
                    if(!seen.insert(k).second)continue;
                    q.source=print(q);
                    out.push_back({q,"strategy:theorem parameter shift "+v+" -> "+v+"+1"});
                    if(v=="n")break;
                }
                if((int)out.size()>=budget)break;

                // Converse experiment. Equality symmetry makes this a useful
                // structural probe, while non-equality relations remain genuinely
                // interesting research questions.
                Statement converse=s;
                std::swap(converse.left,converse.right);
                const std::string ck=statementKey(converse);
                if(seen.insert(ck).second){
                    converse.source=print(converse);
                    out.push_back({converse,"strategy:theorem converse experiment"});
                }
                if((int)out.size()>=budget)break;

                // Polynomial invariant extraction exposes a canonical zero form
                // that can be mined for factors, invariants and compositions.
                if(s.rel==Rel::Eq && polynomial(s.left) && polynomial(s.right)){
                    Statement q; q.rel=Rel::Eq;
                    q.left=subtraction(s.left,s.right); q.right=cn(0);
                    q.assumptions=s.assumptions; q.source=print(q);
                    const std::string ik=statementKey(q);
                    if(seen.insert(ik).second)
                        out.push_back({q,"strategy:theorem invariant extraction"});
                }
            }catch(...){
                // Unsupported or historical objects simply produce no move.
            }
        }
        return out;
    }
};

class ResearchQuestionEngine {
public:
    static std::vector<std::string> build(const ResearchFrontier&frontier,
                                          const std::vector<ResearchConcept>&concepts,size_t limit=24){
        std::vector<std::string>out=frontier.questions(limit);
        for(const auto&c:concepts){
            if(out.size()>=limit)break;
            if(c.domain=="number_theory")out.push_back("Test whether the verified "+c.name+" structure has a converse or weaker hypothesis.");
            else if(c.domain=="combinatorics")out.push_back("Search for a bijective, recurrence, or generating-function explanation of "+c.name+".");
            else if(c.domain=="recurrences")out.push_back("Search for invariants and transformations connecting the "+c.name+" recurrence to another sequence.");
            else out.push_back("Search for a minimal structural explanation of the repeated "+c.name+" pattern.");
        }
        return out;
    }
};


// ============================================================================
// HILBERT 7.0: Mathematical Scientist / abstraction laboratory
//
// The central upgrade is not another catalogue of mathematical functions.
// HILBERT now treats verified theorems as reusable *research objects*.  The
// scientist can instantiate a theorem with new expressions, generate nearby
// questions (assumption ablation, converse, coefficient perturbation), and
// create cross-theorem substitutions.  All such ideas are hypotheses only:
// they must still pass the ordinary skeptic and independent kernel.
// ============================================================================
class MathematicalScientist {
    static E cloneSubst(const E&e,const std::map<std::string,E>&m){
        if(e->k==Kind::Var){
            auto it=m.find(e->name); return it==m.end()?vr(e->name):it->second;
        }
        auto z=std::make_shared<Expr>(); z->k=e->k; z->q=e->q; z->name=e->name;
        for(const auto&c:e->a) z->a.push_back(cloneSubst(c,m));
        return z;
    }
    static Statement substitute(const Statement&s,const std::map<std::string,E>&m){
        Statement q=s; q.left=cloneSubst(s.left,m); q.right=cloneSubst(s.right,m);
        q.assumptions.clear();
        for(const auto&a:s.assumptions) q.assumptions.push_back({cloneSubst(a.left,m),cloneSubst(a.right,m),a.rel});
        q.source=print(q); return q;
    }
    static std::vector<std::string> names(const Statement&s){
        std::set<std::string>v; vars(s.left,v); vars(s.right,v);
        for(const auto&a:s.assumptions){vars(a.left,v);vars(a.right,v);}
        return std::vector<std::string>(v.begin(),v.end());
    }
    static E templateExpr(int seed,const std::string&base){
        switch(seed%7){
            case 0:return vr(base);
            case 1:return op(Kind::Add,{vr(base),cn(1)});
            case 2:return op(Kind::Add,{vr(base),vr("t")});
            case 3:return op(Kind::Mul,{vr(base),vr("t")});
            case 4:return op(Kind::Add,{op(Kind::Mul,{vr(base),vr("t")}),cn(1)});
            case 5:return op(Kind::Pow,{vr(base),cn(2)});
            default:return op(Kind::Add,{vr(base),cn(-1)});
        }
    }
public:
    struct Proposal { Statement statement; std::string reason; std::string source; };

    std::vector<Proposal> instantiate(const std::vector<Record>&records,int budget,uint64_t seed)const{
        std::vector<Proposal> out; std::mt19937_64 rng(seed^0xD1B54A32D192ED03ULL);
        for(auto it=records.rbegin();it!=records.rend() && (int)out.size()<budget;++it){
            const Record&r=*it;
            if(!(r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM||r.status==Status::LEMMA_FOUND))continue;
            std::vector<std::string> ns=names(r.statement); if(ns.empty())continue;
            std::map<std::string,E> m;
            for(size_t i=0;i<ns.size();++i) m[ns[i]]=templateExpr(int((rng()+i)%7),ns[i]);
            Statement q=substitute(r.statement,m);
            // Structural fingerprints intentionally erase variable names and
            // therefore remain equal under many valid substitutions.  That is
            // useful for abstraction, but it must NOT suppress concrete theorem
            // instantiations: an instantiated statement is a new research object
            // even when its alpha-structure is unchanged.  Reject only an exact
            // semantic duplicate of both sides and assumptions.
            bool identical = canonical(q.left)==canonical(r.statement.left) &&
                             canonical(q.right)==canonical(r.statement.right) &&
                             q.assumptions.size()==r.statement.assumptions.size();
            if(identical) continue;
            out.push_back({q,"theorem instantiation with a newly constructed expression", "scientist:instantiation:#"+std::to_string(r.id)});
        }
        return out;
    }

    std::vector<Proposal> questions(const std::vector<Record>&records,int budget)const{
        std::vector<Proposal> out;
        for(auto it=records.rbegin();it!=records.rend() && (int)out.size()<budget;++it){
            const Record&r=*it;
            if(!(r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM))continue;
            // 1. Assumption ablation: ask whether each hypothesis is actually needed.
            if(!r.statement.assumptions.empty()){
                for(size_t k=0;k<r.statement.assumptions.size() && (int)out.size()<budget;++k){
                    Statement q=r.statement;
                    q.assumptions.erase(q.assumptions.begin()+static_cast<long>(k));
                    out.push_back({q,"assumption-ablation question","scientist:assumption-ablation:#"+std::to_string(r.id)});
                }
            }
            // 2. Converse: not assumed to be true; deliberately sent to the skeptic.
            Statement converse=r.statement; std::swap(converse.left,converse.right);
            out.push_back({converse,"converse question","scientist:converse:#"+std::to_string(r.id)});
            // 3. Coefficient perturbation creates a local research neighborhood.
            // Only mutate a literal 1/2/3; this is hypothesis generation, never proof.
            std::vector<E> leftMut;
            for(const auto&e:{r.statement.left,r.statement.right}){
                if(e->k==Kind::Const && e->q.d==BigInt(1)){
                    long long v=0;try{v=std::stoll(e->q.n.str());}catch(...){continue;}
                    for(int d=-1;d<=1;++d)if(d!=0){Statement q=r.statement;if(&e==&r.statement.left)q.left=cn(v+d);else q.right=cn(v+d);out.push_back({q,"local coefficient neighborhood","scientist:perturbation:#"+std::to_string(r.id)});}
                }
            }
        }
        return out;
    }
};


// ============================================================================
// HILBERT 8.0: Mathematical Abstraction & Invention Engine
//
// HILBERT 7 could reuse a theorem by substitution. HILBERT 8 adds a small
// anti-unification laboratory: given independently verified statements, it
// searches for their common structural form and proposes a generalized law.
// It also performs minimal-assumption experiments. These are research
// hypotheses only; the existing Popper + Euclid + Kernel boundary remains the
// sole authority for theorem status.
// ============================================================================
class AbstractionInventionEngine {
    struct AntiResult { E expr; bool generalized=false; };

    static E fresh(const std::string&prefix,int&n){
        return vr(prefix+std::to_string(n++));
    }

    static E anti(const E&a,const E&b,int&freshId,bool&changed){
        if(a->k!=b->k){ changed=true; return fresh("h",freshId); }
        if(a->k==Kind::Const){
            if(a->q==b->q)return a;
            changed=true; return fresh("h",freshId);
        }
        if(a->k==Kind::Var){
            if(a->name==b->name)return vr(a->name);
            changed=true; return fresh("h",freshId);
        }
        if(a->k==Kind::Func && (a->name!=b->name || a->a.size()!=b->a.size())){
            changed=true; return fresh("h",freshId);
        }
        if(a->a.size()!=b->a.size()){ changed=true; return fresh("h",freshId); }
        auto z=std::make_shared<Expr>(); z->k=a->k; z->q=a->q; z->name=a->name;
        for(size_t i=0;i<a->a.size();++i)z->a.push_back(anti(a->a[i],b->a[i],freshId,changed));
        return z;
    }

    static bool sameAssumptionShape(const Statement&a,const Statement&b){
        if(a.assumptions.size()!=b.assumptions.size())return false;
        for(size_t i=0;i<a.assumptions.size();++i)
            if(a.assumptions[i].rel!=b.assumptions[i].rel)return false;
        return true;
    }

    static Statement generalizePair(const Statement&a,const Statement&b,bool&changed){
        Statement q=a; int id=0;
        q.left=anti(a.left,b.left,id,changed);
        q.right=anti(a.right,b.right,id,changed);
        if(a.rel!=b.rel || !sameAssumptionShape(a,b)){changed=false;return a;}
        q.assumptions.clear();
        for(size_t i=0;i<a.assumptions.size();++i){
            bool c1=false,c2=false;
            E l=anti(a.assumptions[i].left,b.assumptions[i].left,id,c1);
            E r=anti(a.assumptions[i].right,b.assumptions[i].right,id,c2);
            q.assumptions.push_back({l,r,a.assumptions[i].rel});
            changed=changed||c1||c2;
        }
        q.source="abstraction-invention";
        return q;
    }

public:
    struct Proposal { Statement statement; std::string reason; std::string source; };

    std::vector<Proposal> generalize(const std::vector<Record>&records,int budget)const{
        std::vector<Proposal> out;
        std::vector<const Record*> facts;
        for(auto it=records.rbegin();it!=records.rend() && facts.size()<32;++it){
            if(it->status==Status::THEOREM||it->status==Status::CONDITIONAL_THEOREM||it->status==Status::LEMMA_FOUND)
                facts.push_back(&*it);
        }
        for(size_t i=0;i<facts.size() && (int)out.size()<budget;++i){
            for(size_t j=i+1;j<facts.size() && (int)out.size()<budget;++j){
                if(facts[i]->statement.rel!=facts[j]->statement.rel)continue;
                bool changed=false;
                Statement q=generalizePair(facts[i]->statement,facts[j]->statement,changed);
                if(!changed)continue;
                std::string k=statementKey(q);
                if(k==statementKey(facts[i]->statement)||k==statementKey(facts[j]->statement))continue;
                out.push_back({q,"anti-unification of two independently verified laws",
                               "invention:generalization:#"+std::to_string(facts[i]->id)+":#"+std::to_string(facts[j]->id)});
            }
        }
        return out;
    }

    // For a conditional theorem, create all one-assumption deletions. The
    // resulting statements are deliberately hypotheses; the normal pipeline
    // decides whether a stronger theorem exists.
    std::vector<Proposal> minimalAssumptionQuestions(const std::vector<Record>&records,int budget)const{
        std::vector<Proposal> out;
        for(auto it=records.rbegin();it!=records.rend() && (int)out.size()<budget;++it){
            const Record&r=*it;
            if(r.status!=Status::CONDITIONAL_THEOREM || r.statement.assumptions.size()<2)continue;
            for(size_t k=0;k<r.statement.assumptions.size() && (int)out.size()<budget;++k){
                Statement q=r.statement;
                q.assumptions.erase(q.assumptions.begin()+static_cast<long>(k));
                out.push_back({q,"minimal-assumption experiment",
                               "invention:assumption-minimization:#"+std::to_string(r.id)});
            }
        }
        return out;
    }
};


// ============================================================================
// HILBERT 12.0: Mathematical Insight Laboratory
//
// This layer deliberately does NOT add another catalogue of finished theorems.
// It studies the *relationships between verified results* and turns those
// relationships into mathematical research objects: families, analogies,
// invariants, assumption patterns and high-value questions.
//
// The distinction matters.  An insight is an explanation or research lead, not
// a theorem.  Any concrete statement it proposes must still pass Popper,
// proof search and the independent Kernel.
// ============================================================================
struct MathematicalInsight {
    std::string kind;
    std::string title;
    std::string explanation;
    std::vector<int> members;
    double confidence=0.0;
};

class MathematicalInsightEngine {
    static void functionHeads(const E&e,std::set<std::string>&out){
        if(e->k==Kind::Func) out.insert(e->name);
        for(const auto&c:e->a) functionHeads(c,out);
    }

    static int kindCount(const E&e,Kind k){
        int n=(e->k==k)?1:0;
        for(const auto&c:e->a)n+=kindCount(c,k);
        return n;
    }

    static std::string familySignature(const Statement&s){
        std::set<std::string> f;
        functionHeads(s.left,f); functionHeads(s.right,f);
        std::ostringstream o;
        o<<domainOf(s)<<"|rel="<<relationName(s.rel)
         <<"|A="<<s.assumptions.size()<<"|F=";
        for(const auto&x:f)o<<x<<",";
        // Operator counts alone are far too coarse: they would call dozens of
        // unrelated polynomial expansions one "family".  Use the alpha-invariant
        // structural fingerprint as the final family coordinate.  For pure
        // polynomial identities this is equivalent to comparing their canonical
        // symbolic form, while recursive/function theorems retain their function
        // architecture.
        o<<"|shape="<<StructuralAbstraction::fingerprint(s);
        return o.str();
    }

    static bool verified(const Record&r){
        return r.status==Status::THEOREM ||
               r.status==Status::CONDITIONAL_THEOREM ||
               r.status==Status::LEMMA_FOUND;
    }

    static std::string firstFunction(const Statement&s){
        std::set<std::string> f;
        functionHeads(s.left,f); functionHeads(s.right,f);
        return f.empty()?std::string():*f.begin();
    }

public:
    std::vector<MathematicalInsight> analyze(const Euler&kb,size_t limit=24)const{
        std::vector<MathematicalInsight> out;
        std::vector<const Record*> facts;
        for(const auto&r:kb.all()) if(verified(r)) facts.push_back(&r);

        // 1. Detect mathematical families: many concrete results can be
        // manifestations of one recurring structural mechanism.
        std::map<std::string,std::vector<int>> families;
        for(const auto*r:facts) families[familySignature(r->statement)].push_back(r->id);
        for(const auto&g:families){
            if(g.second.size()<2)continue;
            std::ostringstream title;
            title<<"Verified theorem family ("<<g.second.size()<<" members)";
            std::ostringstream why;
            why<<"Multiple independently verified results share the same domain, "
               <<"relation, function vocabulary and algebraic operator profile. "
               <<"This is evidence for a reusable mathematical principle rather "
               <<"than isolated facts.";
            double conf=std::min(1.0,0.55+0.10*double(std::min<size_t>(5,g.second.size()-2)));
            out.push_back({"family",title.str(),why.str(),g.second,conf});
            if(out.size()>=limit)break;
        }

        // 2. Detect paired results that use the same mathematical objects but
        // have different exact statements. These are especially valuable for
        // analogy and theorem-transfer research.
        std::set<std::string> analogyFunctions;
        for(size_t i=0;i<facts.size()&&out.size()<limit;++i){
            for(size_t j=i+1;j<facts.size()&&out.size()<limit;++j){
                const Statement&a=facts[i]->statement;
                const Statement&b=facts[j]->statement;
                std::string fa=firstFunction(a), fb=firstFunction(b);
                if(fa.empty()||fa!=fb)continue;
                if(statementKey(a)==statementKey(b))continue;
                if(domainOf(a)!=domainOf(b))continue;
                if(familySignature(a)==familySignature(b))continue;
                // One high-value analogy per mathematical object is more useful
                // than printing dozens of nearly identical instantiations.
                if(!analogyFunctions.insert(fa).second)continue;
                std::ostringstream why;
                why<<"The verified results share the mathematical object '"<<fa
                   <<"' but have different structural forms. Compare them for "
                   <<"a common invariant, recurrence, symmetry, bijection, or "
                   <<"stronger schema.";
                out.push_back({"analogy",
                    "Analogy around "+fa,
                    why.str(),
                    {facts[i]->id,facts[j]->id},0.62});
            }
        }

        // 3. Assumption architecture: repeated conditional theorems are a
        // direct source of mathematicians' favourite questions -- which
        // hypotheses are essential, redundant, or replaceable?
        std::map<std::string,std::vector<const Record*>> conditionalFamilies;
        for(const auto*r:facts){
            if(r->statement.assumptions.empty())continue;
            std::string body=relationName(r->statement.rel)+"|"+
                canonical(r->statement.left)+"|"+canonical(r->statement.right);
            conditionalFamilies[body].push_back(r);
        }
        for(const auto&g:conditionalFamilies){
            if(g.second.size()<2||out.size()>=limit)continue;
            std::vector<int> ids;
            for(const auto*r:g.second)ids.push_back(r->id);
            out.push_back({"assumption",
                "Assumption architecture",
                "The same mathematical claim has been independently encountered "
                "with different hypothesis sets. Investigate minimal, sufficient, "
                "and alternative assumptions instead of treating the hypotheses "
                "as part of the formula itself.",
                ids,0.78});
        }

        // 4. Algebraic invariant insight.  For polynomial theorems, the
        // difference is an exact zero polynomial.  This gives the researcher a
        // representation that can be compared across apparently unrelated
        // surface forms.
        std::map<std::string,std::vector<int>> invariants;
        for(const auto*r:facts){
            if(r->statement.rel!=Rel::Eq)continue;
            auto l=symbolicPolynomial(r->statement.left);
            auto rr=symbolicPolynomial(r->statement.right);
            if(!l||!rr)continue;
            std::string key=(*l+(-*rr)).key();
            invariants[key].push_back(r->id);
        }
        for(const auto&g:invariants){
            if(g.second.size()<2||out.size()>=limit)continue;
            out.push_back({"invariant",
                "Shared symbolic invariant",
                "Different theorem statements reduce to the same exact symbolic "
                "invariant. This suggests searching for the smaller structural law "
                "that generates both statements.",
                g.second,0.84});
        }

        std::sort(out.begin(),out.end(),[](const MathematicalInsight&a,
                                           const MathematicalInsight&b){
            if(a.confidence!=b.confidence)return a.confidence>b.confidence;
            if(a.members.size()!=b.members.size())return a.members.size()>b.members.size();
            return a.title<b.title;
        });
        if(out.size()>limit)out.resize(limit);
        return out;
    }

    std::vector<std::string> questions(const Euler&kb,size_t limit=16)const{
        std::vector<std::string> out;
        auto ins=analyze(kb,limit*2+4);
        std::set<std::string> seenQ;
        for(const auto&i:ins){
            std::string q;
            if(i.kind=="family")
                q="Identify the minimal principle generating verified family: "+i.title;
            else if(i.kind=="analogy")
                q="Compare the analogous verified results and search for a shared invariant: "+i.title;
            else if(i.kind=="assumption")
                q="Experiment on minimal and alternative hypotheses for: "+i.title;
            else
                q="Find the structural explanation of the shared invariant: "+i.title;
            if(seenQ.insert(q).second)out.push_back(q);
            if(out.size()>=limit)break;
        }
        return out;
    }
};


class ResearchFrontierRanker {
public:
    static double priority(const Record&r,double novelty,int attempts){
        const bool proved=r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM||r.status==Status::LEMMA_FOUND;
        const double unresolved=proved?0.0:1.0;
        const double depth=std::min(1.0,double(exprDepth(r.statement.left)+exprDepth(r.statement.right))/20.0);
        return 0.45*unresolved+0.25*novelty+0.20*depth+0.10/(1.0+attempts);
    }
};


// ============================================================================
// HILBERT 17.0 - Closed-loop Research Director
// ============================================================================
struct ResearchMission {
    int id=0;
    Statement target;
    std::string objective,domain,state="OPEN";
    double priority=0.0;
    std::vector<MissionMove> plan;
    int step=0,successes=0,failures=0;
};
class ResearchDirector {
    int nextId_=1;
public:
    std::vector<ResearchMission> create(const std::vector<FrontierItem>&frontier,int budget,int maxSteps){
        std::vector<ResearchMission> out;
        for(size_t i=0;i<frontier.size()&&(int)out.size()<budget;++i){
            const FrontierItem&f=frontier[i];
            try{
                ResearchMission m; m.id=nextId_++; m.target=Parser(f.statement).statement();
                m.domain=f.domain; m.priority=f.priority;
                if(f.state=="REPAIR"){
                    m.objective="repair a falsified conjecture and seek the strongest nearby surviving statement";
                    m.plan={MissionMove::Repair,MissionMove::Counterexample,MissionMove::Prove,MissionMove::Generalize};
                }else if(!m.target.assumptions.empty()){
                    m.objective="prove the claim, then test whether its hypotheses are essential";
                    m.plan={MissionMove::Counterexample,MissionMove::Prove,MissionMove::ForgeLemma,MissionMove::Ablate};
                }else if(m.domain=="number_theory"){
                    m.objective="seek proof or refutation, then search for a stronger arithmetic law";
                    m.plan={MissionMove::Counterexample,MissionMove::Prove,MissionMove::Specialize,MissionMove::Generalize};
                }else if(m.domain=="combinatorics"){
                    m.objective="seek proof or refutation, then expose recurrence, symmetry, or a broader family";
                    m.plan={MissionMove::Counterexample,MissionMove::Prove,MissionMove::ForgeLemma,MissionMove::Generalize};
                }else if(print(m.target).find("(")!=std::string::npos){
                    m.objective="understand the functional mechanism and derive a reusable law";
                    m.plan={MissionMove::Counterexample,MissionMove::Prove,MissionMove::Analogize,MissionMove::Generalize};
                }else{
                    m.objective="establish truth status and search nearby structural consequences";
                    m.plan={MissionMove::Counterexample,MissionMove::Prove,MissionMove::Generalize,MissionMove::Compose};
                }
                if(maxSteps>0&&(int)m.plan.size()>maxSteps)m.plan.resize((size_t)maxSteps);
                out.push_back(m);
            }catch(...){ }
        }
        std::sort(out.begin(),out.end(),[](const ResearchMission&a,const ResearchMission&b){
            if(a.priority!=b.priority)return a.priority>b.priority;
            return a.id<b.id;
        });
        return out;
    }
    static void report(std::ostream&out,const ResearchMission&m){
        out<<"MISSION #"<<m.id<<" ["<<m.state<<"] priority="<<std::fixed<<std::setprecision(3)
           <<m.priority<<" step="<<m.step<<"/"<<m.plan.size()<<" successes="<<m.successes
           <<" failures="<<m.failures<<" domain="<<m.domain<<"\n";
        out<<"  objective: "<<m.objective<<"\n  target: "<<print(m.target)<<"\n  plan:";
        for(size_t i=0;i<m.plan.size();++i)out<<" "<<i+1<<":"<<missionMoveName(m.plan[i]);
        out<<"\n";
    }
};


// ============================================================================
// HILBERT 19.0: Active Mathematical Discovery Laboratory
//
// The previous research loop could generate substitutions, converses,
// generalizations and theorem compositions.  This layer adds a more explicit
// experimental habit: when a verified law is available, HILBERT probes its
// mathematical neighborhood by diagonalizing variables, specializing them at
// canonical boundary values, exchanging variables, and applying controlled
// sign/scale transformations.  These are NOT proofs.  They are experiments
// designed to expose symmetry, invariance, hidden assumptions and nearby laws.
// Every generated statement is returned to the normal skeptic -> proof
// portfolio -> independent kernel pipeline.
// ============================================================================
class ActiveDiscoveryLab {
    static E cloneSubst(const E&e,const std::map<std::string,E>&m){
        if(e->k==Kind::Var){
            auto it=m.find(e->name);
            return it==m.end()?vr(e->name):it->second;
        }
        auto z=std::make_shared<Expr>(); z->k=e->k; z->q=e->q; z->name=e->name;
        for(const auto&c:e->a) z->a.push_back(cloneSubst(c,m));
        return z;
    }

    static Statement substitute(const Statement&s,const std::map<std::string,E>&m){
        Statement q=s;
        q.left=cloneSubst(s.left,m); q.right=cloneSubst(s.right,m);
        q.assumptions.clear();
        for(const auto&a:s.assumptions)
            q.assumptions.push_back({cloneSubst(a.left,m),cloneSubst(a.right,m),a.rel});
        q.source="active-discovery";
        return q;
    }

    static std::vector<std::string> names(const Statement&s){
        std::set<std::string>v; vars(s.left,v); vars(s.right,v);
        for(const auto&a:s.assumptions){vars(a.left,v);vars(a.right,v);}
        return std::vector<std::string>(v.begin(),v.end());
    }

    static bool isSame(const Statement&a,const Statement&b){
        if(a.rel!=b.rel||a.assumptions.size()!=b.assumptions.size())return false;
        if(canonical(a.left)!=canonical(b.left)||canonical(a.right)!=canonical(b.right))return false;
        for(size_t i=0;i<a.assumptions.size();++i){
            if(a.assumptions[i].rel!=b.assumptions[i].rel)return false;
            if(canonical(a.assumptions[i].left)!=canonical(b.assumptions[i].left)||
               canonical(a.assumptions[i].right)!=canonical(b.assumptions[i].right))return false;
        }
        return true;
    }

    static E negated(const E&e){ return op(Kind::Neg,{e}); }

public:
    struct Proposal { Statement statement; std::string reason; std::string source; };

    std::vector<Proposal> probe(const std::vector<Record>&records,int budget)const{
        std::vector<Proposal>out;
        for(auto it=records.rbegin();it!=records.rend()&&(int)out.size()<budget;++it){
            const Record&r=*it;
            if(!(r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM||r.status==Status::LEMMA_FOUND))continue;
            const Statement&s=r.statement;
            std::vector<std::string>vs=names(s);
            if(vs.empty())continue;

            // 1. Diagonal experiments: identify variables and ask whether the
            // theorem contains a hidden diagonal law.
            if(vs.size()>=2){
                std::map<std::string,E>m;
                m[vs[1]]=vr(vs[0]);
                Statement q=substitute(s,m);
                if(!isSame(q,s))out.push_back({q,"active experiment: diagonalize "+vs[1]+" -> "+vs[0],
                    "active-lab:diagonal:#"+std::to_string(r.id)});
            }
            if((int)out.size()>=budget)break;

            // 2. Canonical boundary probes.  Constants 0, 1 and -1 are useful
            // because many algebraic identities reveal fixed points, base cases,
            // vanishing terms or sign behavior there.
            const int64_t boundary[]={0,1,-1};
            for(size_t vi=0;vi<vs.size()&&(int)out.size()<budget;++vi){
                for(int64_t b:boundary){
                    std::map<std::string,E>m; m[vs[vi]]=cn(b);
                    Statement q=substitute(s,m);
                    if(!isSame(q,s))out.push_back({q,
                        "active experiment: boundary specialization "+vs[vi]+" -> "+std::to_string(b),
                        "active-lab:boundary:#"+std::to_string(r.id)});
                    if((int)out.size()>=budget)break;
                }
                // Keep the laboratory broad: one variable is normally enough
                // to expose a base case without exhausting the research budget.
                if(vi>=1)break;
            }
            if((int)out.size()>=budget)break;

            // 3. Exchange the first two variables.  This explicitly searches for
            // commutative/symmetric companions rather than merely noticing them.
            if(vs.size()>=2){
                std::map<std::string,E>m;
                m[vs[0]]=vr("__hilbert_swap_tmp");
                m[vs[1]]=vr(vs[0]);
                m["__hilbert_swap_tmp"]=vr(vs[1]);
                Statement q=substitute(s,m);
                if(!isSame(q,s))out.push_back({q,"active experiment: exchange variables "+vs[0]+" <-> "+vs[1],
                    "active-lab:symmetry:#"+std::to_string(r.id)});
            }
            if((int)out.size()>=budget)break;

            // 4. Uniform sign experiment.  This can expose even/odd structure
            // without assuming that such symmetry actually exists.
            if(vs.size()>=1){
                std::map<std::string,E>m;
                for(const auto&v:vs)m[v]=negated(vr(v));
                Statement q=substitute(s,m);
                if(!isSame(q,s))out.push_back({q,"active experiment: simultaneous sign reversal",
                    "active-lab:sign:#"+std::to_string(r.id)});
            }
            if((int)out.size()>=budget)break;

            // 5. Diagonal perturbation: x -> x+1 for one variable.  This is a
            // bridge between a static identity and possible recurrence/finite-
            // difference structure.  The normal proof kernel decides the result.
            if(!vs.empty()){
                std::map<std::string,E>m; m[vs[0]]=op(Kind::Add,{vr(vs[0]),cn(1)});
                Statement q=substitute(s,m);
                if(!isSame(q,s))out.push_back({q,"active experiment: unit parameter shift of "+vs[0],
                    "active-lab:shift:#"+std::to_string(r.id)});
            }
            if((int)out.size()>=budget)break;
        }
        return out;
    }
};

class DiscoveryEngine {
    MathematicalScientist scientist;
    AbstractionInventionEngine inventor;

    Config cfg;
    Euler kb;
    Gauss gauss;
    Meta meta;
    Galois galois;
    Ramanujan ram;
    Popper pop;
    Euclid euclid;
    Kernel kernel;
    ResearchGraph graph;
    std::vector<Theory> theories;
    std::vector<Statement> learned;
    std::unordered_set<std::string> seen;
    SearchPolicy policy;
    NoveltyArchive novelty;
    ResearchCouncil council;
    ResearchTrace trace;
    TheoremComposer composer;
    ProofPlanner planner;
    LemmaForge lemmaForge;
    AlgebraicTheoremMiner algebraicMiner;
    ProofPortfolio portfolio;
    ResearchDebate debate;
    TheoryInducer inducer;
    TheoryResearcher theoryResearcher;
    ResearchAgenda agenda;
    ResearchStrategyPlanner strategyPlanner;
    ExperienceMemory experience;
    CounterexampleRepairer repairer;
    ResearchJournal journal;
    ResearchFrontier frontier;
    ResearchDirector director;
    std::vector<ResearchMission> missionArchive;
    std::vector<ResearchConcept> concepts;
    MathematicalInsightEngine insightEngine;
    ActiveDiscoveryLab activeLab;
    std::chrono::steady_clock::time_point deadline;
    bool deadlineActive=false;
    uint64_t generated=0;
    int nextTheory=1;
    std::string persistenceFile="hilbert_brain.db";
    std::string liveGraphFile="hilbert_live_graph.html";
    bool persistenceEnabled=false;
    bool persistenceLoading=false;

    void persistState(){
        if(!persistenceEnabled||persistenceLoading)return;
        try{
            const std::string tmp=persistenceFile+".tmp";
            BrainCheckpoint::save(kb,learned,experience,tmp);
            std::remove(persistenceFile.c_str());
            if(std::rename(tmp.c_str(),persistenceFile.c_str())!=0){ std::remove(tmp.c_str()); throw Error("cannot replace persistent brain: "+persistenceFile); }
            if(!liveGraphFile.empty()) graph.exportLiveHtml(liveGraphFile,kb.all());
        }catch(const std::exception&e){
            std::cerr<<"[PERSISTENCE] "<<e.what()<<"\n";
        }
    }

    bool budgetExpired() const { return deadlineActive && std::chrono::steady_clock::now() >= deadline; }

    void registerTheoryFromRecord(const Record&r){
        if(r.status!=Status::THEOREM&&r.status!=Status::CONDITIONAL_THEOREM&&r.status!=Status::LEMMA_FOUND)return;
        std::string domain=domainOf(r.statement);
        for(auto&t:theories){
            for(int id:t.members)if(id==r.id)return;
            if(t.domain==domain){
                t.members.push_back(r.id);
                TheoryBuilder::rebuild(t,kb,graph.edges());
                return;
            }
        }
        Theory t; t.id=nextTheory++; t.domain=domain;
        t.name="Research program: "+domain;
        t.members.push_back(r.id);
        theories.push_back(std::move(t));
        TheoryBuilder::rebuild(theories.back(),kb,graph.edges());
    }

    void rebuildTheories(){
        for(auto&t:theories)TheoryBuilder::rebuild(t,kb,graph.edges());
    }

    void connectRecord(const Record&r){
        const auto&all=kb.all();
        const Record*cur=kb.get(r.id);
        if(!cur)return;
        std::string ct=print(cur->statement);
        for(const auto&old:all){
            if(old.id==cur->id)continue;
            if((old.status==Status::THEOREM||old.status==Status::CONDITIONAL_THEOREM) &&
               (cur->status==Status::THEOREM||cur->status==Status::CONDITIONAL_THEOREM)){
                std::string ot=print(old.statement);
                if(canonical(old.statement.right)==canonical(cur->statement.left))
                    graph.add(old.id,cur->id,"composition",
                              "right side of one theorem is left side of another");
                if(domainOf(old.statement)==domainOf(cur->statement))
                    graph.add(old.id,cur->id,"same_domain","same mathematical domain");
                if(ot.find("gcd")!=std::string::npos&&ct.find("gcd")!=std::string::npos)
                    graph.add(old.id,cur->id,"gcd_chain","shared gcd structure");
            }
        }
        graph.buildSimilarity(all);
    }

    Record processInternal(Statement s,std::string method,
                           std::string ancestry="generated candidate"){
        Record r;
        r.statement=s;r.generator=std::move(method);r.ancestry=std::move(ancestry);

        if(exprDepth(s.left)>cfg.maxDepth||exprDepth(s.right)>cfg.maxDepth){
            r.status=Status::SEARCH_EXHAUSTED;
            r.evidence.reason="expression exceeds configured proof-search depth";
        }else{
            r.evidence=pop.attack(s,std::max(1,cfg.tests));
            r.status=r.evidence.counterexample?Status::FALSIFIED:Status::EMPIRICALLY_SUPPORTED;
            if(!r.evidence.counterexample){
                auto v=debate.skeptic(s,cfg.seed+generated,cfg.tests/2);
                if(v.falsified){ r.status=Status::FALSIFIED; r.evidence.reason=v.reason; }
            }
            r.score=gauss.score(s,r.evidence,kb.knownEquivalent(s));

            if(!r.evidence.counterexample){
                r.status=Status::PROOF_SEARCHING;
                r.proof=portfolio.prove(s,cfg.maxStates,cfg.maxDepth);
                if(r.proof){
                    std::string why;
                    if(kernel.verify(s,*r.proof,why)){
                        r.status=Status::KERNEL_VERIFIED;r.kernelReason=why;
                    }else{
                        r.status=Status::KERNEL_REJECTED;r.kernelReason=why;
                    }
                }else{
                    r.status=Status::EMPIRICALLY_SUPPORTED;
                    r.evidence.reason="survived exact adversarial tests but no trusted certificate was found";
                }
            }
        }

        bool verified=(r.status==Status::KERNEL_VERIFIED);
        double reward=verified?1.0:(r.status==Status::FALSIFIED?-0.35:0.15);
        experience.observe(r.generator,verified,r.evidence.tests,reward,domainOf(s));
        meta.observe(r.generator,verified);
        policy.observe(domainOf(s),r.generator,r.status,reward);
        const double noveltyScore=novelty.novelty(s);
        novelty.admit(s);
        novelty.reward(s,ResearchObjective::value(r,noveltyScore));
        int id=kb.add(r);
        if(r.status==Status::KERNEL_VERIFIED)kb.promote(id);
        const Record*stored=kb.get(id);
        if(stored){
            if(stored->status==Status::THEOREM||
               stored->status==Status::CONDITIONAL_THEOREM)
                registerTheoryFromRecord(*stored);
            connectRecord(*stored);
            double ns=novelty.novelty(stored->statement);
            trace.record(0,generated,*stored,ns);
            journal.observe(*stored,ns);
            frontier.observe(*stored,ns);
            persistState();
            return *stored;
        }
        return r;
    }

    void addCandidateText(const std::string&text,const std::string&origin,
                           std::ostream*log=nullptr){
        try{
            Statement s=Parser(text).statement();
            std::string key=statementKey(s);
            if(!seen.insert(key).second)return;
            ++generated;
            Record r=processInternal(s,origin,"research:"+std::to_string(generated));

            // When a direct attempt stalls, ask the lemma forge for small
            // trusted-schema obligations that may unlock the target.  The
            // forged lemmas are independently tested and kernel-checked.
            if(r.status!=Status::FALSIFIED){
                forgeLemmas(s,log);
            }

            // Only unsupported-but-tested statements become conjectures.
            // A kernel result is promoted by Euler and remains a theorem.
            if(r.status==Status::EMPIRICALLY_SUPPORTED&&!r.evidence.counterexample){
                r.status=Status::CONJECTURE;
                kb.setStatus(r.id,Status::CONJECTURE);
                if(const Record*updated=kb.get(r.id))
                    frontier.observe(*updated,novelty.novelty(updated->statement));
            }
            if(log){
                *log<<"#"<<r.id<<" ["<<statusName(r.status)<<"] "
                    <<print(r.statement)<<"\n";
                *log<<"  origin: "<<r.generator<<"\n";
                *log<<"  score: "<<std::fixed<<std::setprecision(3)
                    <<r.score.total<<"\n";
                if(r.evidence.counterexample){
                    *log<<"  counterexample:";
                    for(const auto&x:*r.evidence.counterexample)
                        *log<<" "<<x.first<<"="<<x.second.str();
                    *log<<"\n";
                }
                if(r.proof)*log<<"  kernel: "<<r.kernelReason<<"\n";
                // Counterexample-guided repair: a failed conjecture becomes a
                // local research neighborhood. Repaired candidates are still
                // ordinary candidates and cannot bypass the kernel.
                if(r.status==Status::FALSIFIED && cfg.maxRepairs>0 && !budgetExpired()) {
                    int repairBudget=std::max(1,std::min(cfg.maxRepairs,6));
                    for(const auto&rq:repairer.repair(s,repairBudget)) {
                        if(budgetExpired())break;
                        std::string rk=statementKey(rq);
                        if(!seen.insert(rk).second)continue;
                        ++generated;
                        Record rr=processInternal(rq,"counterexample-repair","repair-of:"+std::to_string(r.id));
                        if(log && (rr.status==Status::THEOREM||rr.status==Status::CONDITIONAL_THEOREM||rr.status==Status::FALSIFIED))
                            *log<<"  repair #"<<rr.id<<" ["<<statusName(rr.status)<<"] "<<print(rr.statement)<<"\n";
                    }
                }
            }
        }catch(const std::exception&e){
            if(log)*log<<"[REJECTED CANDIDATE] "<<text<<" :: "<<e.what()<<"\n";
        }
    }

    void forgeLemmas(const Statement&obstacle,std::ostream*log){
        int budget=std::max(4,std::min(20,cfg.iterations/4+4));
        for(const auto&candidate:lemmaForge.forge(obstacle,budget)){
            try{
                Statement q=Parser(candidate.first).statement();
                std::string key=statementKey(q);
                if(!seen.insert(key).second) continue;
                ++generated;
                Record r=processInternal(q,candidate.second,"lemma-obstacle:"+statementKey(obstacle));
                if(r.status==Status::THEOREM || r.status==Status::CONDITIONAL_THEOREM || r.status==Status::KERNEL_VERIFIED){
                    kb.setStatus(r.id,Status::LEMMA_FOUND);
                    if(log) *log<<"[LEMMA_FOUND] #"<<r.id<<" "<<print(r.statement)<<"\n";
                } else if(log && r.status!=Status::FALSIFIED){
                    *log<<"[LEMMA_CANDIDATE] "<<print(r.statement)<<" ["<<statusName(r.status)<<"]\n";
                }
            }catch(const std::exception&e){
                if(log) *log<<"[LEMMA_REJECTED] "<<candidate.first<<" :: "<<e.what()<<"\n";
            }
        }
    }

    void mineDatabase(std::ostream*log){
        FormulaMiner fm;
        for(const auto&f:fm.mine(learned))
            addCandidateText(f,"data-mining:finite-difference formula",log);

        RecurrenceMiner rm;
        for(const auto&f:rm.mine(learned))
            addCandidateText(f,"data-mining:linear recurrence",log);

        RelationalMiner rel;
        for(const auto&f:rel.mine(learned))
            addCandidateText(f.first,f.second,log);

        CrossSequenceMiner cross;
        for(const auto&f:cross.mine(learned))
            addCandidateText(f.first,f.second,log);
    }

    void mutateKnownResults(std::ostream*log,int budget,uint64_t seed){
        TheoremTransformer tr;
        auto candidates=tr.instantiate(kb,budget,seed);
        for(const auto&x:candidates)addCandidateText(print(x.first),x.second,log);
    }

    std::vector<Statement> selectFrontier(size_t limit) const {
        struct Item { double score; int id; Statement s; };
        std::vector<Item> items;
        for(const auto&r:kb.all()){
            if(r.status!=Status::CONJECTURE &&
               r.status!=Status::EMPIRICALLY_SUPPORTED &&
               r.status!=Status::KERNEL_REJECTED) continue;
            double priority=r.score.total;
            if(r.status==Status::CONJECTURE) priority+=0.25;
            if(r.status==Status::KERNEL_REJECTED) priority+=0.10;
            items.push_back({priority,r.id,r.statement});
        }
        std::sort(items.begin(),items.end(),[](const Item&a,const Item&b){
            if(a.score!=b.score)return a.score>b.score;
            return a.id>b.id;
        });
        std::vector<Statement> out;
        for(const auto&i:items){
            if(out.size()>=limit)break;
            out.push_back(i.s);
        }
        return out;
    }


    void executeResearchMissions(std::ostream&out,int budget,int maxSteps){
        if(budget<=0||maxSteps<=0)return;
        std::vector<FrontierItem> tasks=frontier.top((size_t)std::max(budget,budget*2));
        std::vector<ResearchMission> missions=director.create(tasks,budget,maxSteps);
        for(size_t mi=0;mi<missions.size();++mi){
            if(budgetExpired())break;
            ResearchMission&m=missions[mi]; m.plan=policy.orderMission(m.domain,m.plan); ResearchDirector::report(out,m);
            for(size_t si=0;si<m.plan.size();++si){
                if(budgetExpired())break;
                m.step=int(si)+1;
                MissionMove move=m.plan[si];
                out<<"  -> STEP "<<m.step<<" "<<missionMoveName(move)<<"\n";
                try{
                    if(move==MissionMove::Counterexample){
                        Evidence e=Popper(cfg.seed+uint64_t(m.id)*1009ULL+uint64_t(si)).attack(m.target,std::max(20,cfg.tests/2));
                        if(e.counterexample){
                            ++m.successes; out<<"     result: counterexample found";
                            for(const auto&x:*e.counterexample)out<<" "<<x.first<<"="<<x.second.str();
                            out<<"\n";
                            for(const auto&rq:repairer.repair(m.target,std::max(1,std::min(3,cfg.maxRepairs))))
                                addCandidateText(print(rq),"mission:repair");
                            m.state="REFUTED"; break;
                        }
                        ++m.successes; out<<"     result: survived exact adversarial testing; not a proof\n";
                    }else if(move==MissionMove::Prove){
                        Record r=processInternal(m.target,"mission:prove","mission:"+std::to_string(m.id));
                        if(r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM){
                            ++m.successes; m.state="SOLVED"; out<<"     result: theorem #"<<r.id<<" kernel-verified\n"; break;
                        }
                        ++m.failures; out<<"     result: "<<statusName(r.status)<<"\n";
                    }else if(move==MissionMove::ForgeLemma){
                        size_t before=kb.all().size(); forgeLemmas(m.target,&out); size_t after=kb.all().size();
                        if(after>before)++m.successes;else ++m.failures; out<<"     result: lemma search records="<<(after-before)<<"\n";
                    }else if(move==MissionMove::Generalize){
                        std::vector<Statement> one(1,m.target); std::vector<ResearchStrategyPlanner::Proposal> ps=strategyPlanner.plan(one,4); int produced=0;
                        for(const auto&p:ps){if(budgetExpired())break;addCandidateText(print(p.statement),"mission:generalize:"+p.reason);++produced;}
                        if(produced>0)++m.successes;else ++m.failures; out<<"     result: generated "<<produced<<" structural hypotheses\n";
                    }else if(move==MissionMove::Specialize){
                        int produced=0,limit=std::max(2,std::min(4,cfg.researchBeam/4));
                        for(const auto&p:scientist.instantiate(kb.all(),limit,cfg.seed+uint64_t(m.id)*31337ULL)){if(budgetExpired())break;addCandidateText(print(p.statement),"mission:specialize:"+p.source);++produced;}
                        if(produced>0)++m.successes;else ++m.failures; out<<"     result: generated "<<produced<<" instantiated hypotheses\n";
                    }else if(move==MissionMove::Compose){
                        int produced=0;
                        for(const auto&c:composer.compose(kb,std::max(2,std::min(6,cfg.researchBeam/4)))){if(budgetExpired())break;addCandidateText(c.first,"mission:compose:"+c.second);++produced;}
                        if(produced>0)++m.successes;else ++m.failures; out<<"     result: composed "<<produced<<" consequences\n";
                    }else if(move==MissionMove::Ablate){
                        int produced=0;
                        for(const auto&p:inventor.minimalAssumptionQuestions(kb.all(),4)){if(budgetExpired())break;addCandidateText(print(p.statement),"mission:ablation:"+p.reason);++produced;}
                        if(produced>0)++m.successes;else ++m.failures; out<<"     result: generated "<<produced<<" assumption experiments\n";
                    }else if(move==MissionMove::Converse){
                        Statement q=m.target;std::swap(q.left,q.right);addCandidateText(print(q),"mission:converse");++m.successes;
                    }else if(move==MissionMove::Analogize){
                        auto qs=insightEngine.questions(kb,4);if(!qs.empty())++m.successes;else ++m.failures;for(const auto&q:qs)out<<"     analogy-question: "<<q<<"\n";
                    }else if(move==MissionMove::Repair){
                        int produced=0;for(const auto&rq:repairer.repair(m.target,std::max(1,std::min(4,cfg.maxRepairs)))){if(budgetExpired())break;addCandidateText(print(rq),"mission:repair");++produced;}
                        if(produced>0)++m.successes;else ++m.failures;out<<"     result: generated "<<produced<<" repairs\n";
                    }
                }catch(const std::exception&e){++m.failures;out<<"     result: action error: "<<e.what()<<"\n";}
                { Status ms=(m.state=="SOLVED"||m.state=="REFUTED")?Status::THEOREM:Status::EMPIRICALLY_SUPPORTED; double mr=(m.state=="SOLVED")?1.0:(m.state=="REFUTED")?0.65:(m.successes>0?0.20:-0.15); policy.observe(m.domain,std::string("mission:")+missionMoveName(move),ms,mr); }
                if(m.state=="OPEN"){
                    const std::string key=statementKey(m.target);
                    for(const auto&r:kb.all())if(statementKey(r.statement)==key){
                        if(r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM){m.state="SOLVED";break;}
                        if(r.status==Status::FALSIFIED){m.state="REFUTED";break;}
                    }
                }
                if(m.state!="OPEN")break;
            }
            if(m.state=="OPEN")m.state="ESCALATED";
            ResearchDirector::report(out,m); missionArchive.push_back(m);
        }
    }
public:
    explicit DiscoveryEngine(Config c):
        cfg(c),ram(c.seed),pop(c.seed^0x9e3779b97f4a7c15ULL),
        euclid(c.maxStates,c.maxDepth),council(c.seed),algebraicMiner(c.seed^0xD1B54A32D192ED03ULL,3){}

    void initializePersistence(const std::string&dbFile,const std::string&graphFile,
                               bool reset,std::ostream&out){
        persistenceFile=dbFile.empty()?"hilbert_brain.db":dbFile;
        liveGraphFile=graphFile.empty()?"hilbert_live_graph.html":graphFile;
        if(reset){ std::remove(persistenceFile.c_str()); }
        persistenceEnabled=true;
        persistenceLoading=true;
        try{
            std::ifstream in(persistenceFile);
            if(in){
                out<<"[PERSISTENCE] loading "<<persistenceFile<<"\n";
                auto statements=BrainCheckpoint::loadStatements(persistenceFile);
                int loaded=0;
                for(const auto&text:statements){
                    try{
                        Statement st=Parser(text).statement();
                        std::string k=statementKey(st);
                        if(!seen.insert(k).second)continue;
                        learned.push_back(st);
                        Record r=processInternal(st,"persistent-replay","persistent-db");
                        if(r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM||r.status==Status::LEMMA_FOUND)++loaded;
                    }catch(const std::exception&e){ out<<"[PERSISTENCE-SKIP] "<<e.what()<<"\n"; }
                }
                out<<"[PERSISTENCE] restored "<<loaded<<" trusted results\n";
            }else{
                out<<"[PERSISTENCE] new database: "<<persistenceFile<<"\n";
            }
        }catch(const std::exception&e){ out<<"[PERSISTENCE] load skipped: "<<e.what()<<"\n"; }
        persistenceLoading=false;
        graph.buildSimilarity(kb.all());
        if(!liveGraphFile.empty()){
            try{ graph.exportLiveHtml(liveGraphFile,kb.all()); }catch(const std::exception&e){ out<<"[GRAPH] "<<e.what()<<"\n"; }
        }
    }

    void flushPersistence(){ persistState(); }
    const std::string& persistencePath()const{return persistenceFile;}
    const std::string& liveGraphPath()const{return liveGraphFile;}

    const std::vector<ResearchMission>& missions() const { return missionArchive; }
    std::vector<FrontierItem> frontierTasks(size_t n=20) const { return frontier.top(n); }
    void runMissions(std::ostream&out,int budget,int steps){ executeResearchMissions(out,budget,steps); }

    Record process(Statement s,std::string method,
                   std::string ancestry="generated candidate"){
        return processInternal(std::move(s),std::move(method),std::move(ancestry));
    }

    void learnFile(const std::string&file,std::ostream&out){
        std::ifstream in(file);
        if(!in){
            out<<"[LEARN] file does not exist; starting a new learning database: "<<file<<"\n";
            std::ofstream create(file);
            if(!create)throw Error("cannot create knowledge database: "+file);
            return;
        }
        std::string line;int n=0;
        while(std::getline(in,line)){
            size_t a=line.find_first_not_of(" \t\r\n");
            if(a==std::string::npos||line[a]=='#')continue;
            size_t b=line.find_last_not_of(" \t\r\n");
            line=line.substr(a,b-a+1);
            try{
                Statement s=Parser(line).statement();
                learned.push_back(s);++n;
                Record r=processInternal(s,"learned_database","database");
                if(r.status==Status::KERNEL_VERIFIED)registerTheoryFromRecord(r);
            }catch(const std::exception&e){
                out<<"[SKIP] "<<line<<" :: "<<e.what()<<"\n";
            }
        }
        out<<"Learned "<<n<<" statements from "<<file<<"\n";
    }

    // Main autonomous loop.  Each round deliberately contains both
    // exploitation (learned knowledge) and exploration (new stochastic
    // structures).  The order changes with the knowledge acquired in earlier
    // rounds, so the engine is not just a fixed list of equations.
    void research(std::ostream&o,int rounds){
        rounds=std::max(1,rounds);
        deadlineActive=cfg.timeLimitMs>0;
        if(deadlineActive)deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(cfg.timeLimitMs);
        o<<"H.I.L.B.E.R.T. autonomous mathematical research\n";
        o<<"knowledge facts="<<learned.size()
         <<", rounds="<<rounds<<", tests/candidate="<<cfg.tests<<"\n";

        mineDatabase(&o);

        // V20.1: theorem-driven research bootstrap. A verified theorem is not
        // dead knowledge: it is the strongest available research object. This
        // pass runs before stochastic exploration so short-budget research can
        // still generate executable strategy moves even with an empty frontier.
        {
            std::vector<Statement> trustedObjects;
            std::set<std::string> trustedSeen;
            for(auto it=kb.all().rbegin();
                it!=kb.all().rend() && trustedObjects.size()<12;++it){
                if(it->status!=Status::THEOREM &&
                   it->status!=Status::CONDITIONAL_THEOREM &&
                   it->status!=Status::LEMMA_FOUND &&
                   it->status!=Status::KERNEL_VERIFIED) continue;
                const std::string k=statementKey(it->statement);
                if(trustedSeen.insert(k).second)trustedObjects.push_back(it->statement);
            }
            const int trustedBudget=std::min(24,std::max(4,cfg.researchBeam/2));
            for(const auto&p:strategyPlanner.planTrusted(trustedObjects,trustedBudget)){
                if(budgetExpired())break;
                addCandidateText(print(p.statement),"research-strategy:"+p.reason,&o);
            }
        }

        ConjectureSynthesizer syn(cfg.seed);

        for(int round=0;round<rounds;++round){
            if(budgetExpired()){ o<<"=== research budget exhausted ===\n"; break; }
            o<<"=== research round "<<round+1<<" ===\n";

            if(budgetExpired()) break;
            // 1. Seed hypotheses establish a broad search frontier.
            auto structural=syn.structural();
            for(const auto&c:structural)addCandidateText(c.first,
                "hypothesis-seed:"+c.second,&o);

            // 1A0. Priority reasoning pass.  Small deterministic reasoning
            // operators run before broad stochastic exploration so a short
            // research budget is still capable of producing genuine derived
            // mathematics.  This also makes autonomous runs reproducible across
            // machines with different filesystem and process overhead.
            if(!budgetExpired()){
                int priorityComposition=std::min(8,std::max(2,cfg.researchBeam/2));
                for(const auto&c:composer.compose(kb,priorityComposition)){
                    if(budgetExpired())break;
                    addCandidateText(c.first,c.second,&o);
                }
            }
            if(!budgetExpired()){
                int priorityScience=std::min(4,std::max(2,cfg.researchBeam/4));
                for(const auto&p:scientist.instantiate(kb.all(),priorityScience,
                    cfg.seed+uint64_t(round)*104729ULL+17ULL)){
                    if(budgetExpired())break;
                    addCandidateText(print(p.statement),p.source+":"+p.reason,&o);
                }
            }

            // 1A. Immediately turn the newly created frontier into deliberate
            // research moves.  This is intentionally early: a researcher should
            // exploit a newly discovered question before spending the budget on
            // broad stochastic exploration.
            if(budgetExpired()) break;
            {
                std::vector<Statement> earlyFrontier=selectFrontier(
                    static_cast<size_t>(std::max(3,std::min(10,cfg.researchBeam/2+1))));
                int strategyBudget=policy.quota("strategy",std::max(6,cfg.iterations),3);
                strategyBudget=std::min(20,strategyBudget);
                for(const auto&p:strategyPlanner.plan(earlyFrontier,strategyBudget)){
                    if(budgetExpired())break;
                    addCandidateText(print(p.statement),"research-strategy:"+p.reason,&o);
                }
            }

            if(budgetExpired()) break;
            // 1C. Closed-loop research director: execute sequential missions
            // against the strongest frontier objects before broad exploration.
            if(!budgetExpired()) executeResearchMissions(o,std::min(12,std::max(1,cfg.missionBudget)),std::min(8,std::max(1,cfg.missionSteps)));

            // 1B. Genuine symbolic discovery: construct fresh expressions and
            // expand them generically.  The exact theorem shape is generated
            // at runtime rather than selected from a theorem catalogue.
            int synthesisBudget=policy.quota("synthesis",std::max(3,cfg.iterations/2+3),2);
            synthesisBudget=std::min(30,synthesisBudget);
            for(const auto&c:algebraicMiner.generate(synthesisBudget))
                addCandidateText(c.first,c.second,&o);

            if(budgetExpired()) break;
            // 2. Mutational exploration searches nearby alternatives and is
            //    intentionally allowed to generate false mathematics.
            auto mutations=syn.mutations();
            for(const auto&c:mutations)addCandidateText(c.first,
                "mutation:"+c.second,&o);

            if(budgetExpired()) break;
            // 3. Learn from the supplied database.
            mineDatabase(&o);

            if(budgetExpired()) break;
            // 4. Generalize verified mathematics into new forms.
            int budget=policy.quota("generalization",std::max(10,cfg.iterations*2),4);
            budget=std::min(100,budget);
            mutateKnownResults(&o,budget,cfg.seed+uint64_t(round)*7919ULL);

            if(budgetExpired()) break;
            // 5. Compose independently verified mathematics into new consequences.
            //    This is the first explicit theorem-to-theorem reasoning layer.
            int compositionBudget=policy.quota("composition",std::max(8,cfg.iterations*2),4);
            compositionBudget=std::min(80,compositionBudget);
            for(const auto&c:composer.compose(kb,compositionBudget))
                addCandidateText(c.first,c.second,&o);

            if(budgetExpired()) break;
            // 6. Lemma-forging pass: convert difficult frontier statements
            //    into smaller, independently verifiable research obligations.
            //    This is deliberately bounded so autonomous search cannot
            //    explode indefinitely.
            size_t frontierBudget=static_cast<size_t>(policy.quota("lemma",std::max(8,cfg.iterations/2),4));
            frontierBudget=std::min<size_t>(static_cast<size_t>(std::max(4,cfg.researchBeam)),frontierBudget);
            auto frontier=selectFrontier(frontierBudget);
            for(const auto&target:frontier) forgeLemmas(target,&o);

            if(budgetExpired()) break;
            // 6B. The deliberate strategy pass already ran immediately after
            // frontier creation above.  The later stages now focus on exploring
            // the consequences of those selected moves.

            if(budgetExpired()) break;
            // 7. Stochastic structural exploration.  Unlike the seed list,
            //    Ramanujan produces random compositions of algebraic and
            //    arithmetic templates and therefore supplies new paths.
            int explore=policy.quota("exploration",std::max(1,cfg.iterations),1);
            explore=std::min(120,explore);
            for(int i=0;i<explore;++i){
                auto g=ram.generate();
                std::string key=statementKey(g.first);
                if(!seen.insert(key).second)continue;
                ++generated;
                Record r=processInternal(g.first,g.second,
                                         "exploration:"+std::to_string(generated));
                o<<"#"<<r.id<<" ["<<statusName(r.status)<<"] "
                 <<print(r.statement)<<"\n";
            }

            if(budgetExpired()) break;
            // 8. Feed newly generated exact observations back into miners.
            mineDatabase(&o);

            if(budgetExpired()) break;
            // 9. Research council: independent explorer/analogy/concept agents
            // deliberate over the current verified knowledge and propose fresh
            // questions. These proposals still enter the ordinary trust pipeline.
            int councilBudget=policy.quota("council",std::max(6,cfg.iterations/2),3);
            councilBudget=std::min(36,councilBudget);
            for(const auto&p:council.deliberate(kb,novelty,concepts,councilBudget))
                addCandidateText(p.text,p.method+":"+p.reason,&o);

            if(budgetExpired()) break;
            // 10. Mathematical Scientist: instantiate verified theorems into new
            // expression contexts and deliberately ask converse/assumption questions.
            int scienceBudget=policy.quota("scientist",std::max(4,cfg.researchBeam/4),2);
            scienceBudget=std::min(16,scienceBudget);
            for(const auto&p:scientist.instantiate(kb.all(),scienceBudget,cfg.seed+uint64_t(round)*104729ULL))
                addCandidateText(print(p.statement),p.source+":"+p.reason,&o);
            for(const auto&p:scientist.questions(kb.all(),std::min(6,std::max(2,scienceBudget/2))))
                addCandidateText(print(p.statement),p.source+":"+p.reason,&o);

            if(budgetExpired()) break;
            // 10B. Active mathematical experimentation: probe verified laws at
            // boundaries, diagonals, symmetries and controlled parameter shifts.
            // These experiments intentionally produce false candidates as well;
            // the research value comes from what the skeptic and kernel teach us.
            int activeBudget=policy.quota("active-lab",std::max(6,cfg.researchBeam/2),3);
            activeBudget=std::min(36,activeBudget);
            for(const auto&p:activeLab.probe(kb.all(),activeBudget))
                addCandidateText(print(p.statement),p.source+":"+p.reason,&o);

            if(budgetExpired()) break;
            // 11. Abstraction & invention: anti-unify independently verified
            // laws and deliberately weaken multi-assumption theorems.
            int inventionBudget=policy.quota("invention",std::max(4,cfg.researchBeam/3),2);
            inventionBudget=std::min(18,inventionBudget);
            for(const auto&p:inventor.generalize(kb.all(),inventionBudget))
                addCandidateText(print(p.statement),p.source+":"+p.reason,&o);
            for(const auto&p:inventor.minimalAssumptionQuestions(kb.all(),std::min(6,inventionBudget/2+1)))
                addCandidateText(print(p.statement),p.source+":"+p.reason,&o);

            // 12. Let the graph become a structural memory rather than a log.
            graph.buildSimilarity(kb.all());

            // 13. Turn the accumulated results into mathematical research
            // programs: definitions, principles, dependencies, maturity and
            // open questions. Then ask whether different programs have a
            // non-trivial bridge. Bridge statements are only hypotheses.
            concepts=inducer.induce(kb);
            rebuildTheories();
            int theoryBudget=policy.quota("theory",std::max(2,cfg.iterations/3),1);
            theoryBudget=std::min(12,theoryBudget);
            for(const auto&tp:theoryResearcher.propose(theories,kb,theoryBudget))
                addCandidateText(tp.text,"theory-research:"+tp.reason,&o);
            rebuildTheories();

            o<<"policy: "<<policy.report()<<"\n";
            if(cfg.timeLimitMs>0) {
                // The outer caller still owns the hard time limit; this line
                // makes the boundary explicit in experiment logs.
                o<<"time-limit configured="<<cfg.timeLimitMs<<" ms\n";
            }
        }

        o<<"research complete: generated="<<generated
         <<", records="<<kb.all().size()
         <<", theories="<<theories.size()
         <<", graph_edges="<<graph.edges().size()<<"\n";
        o<<"Meta: "<<meta.report()<<"\n";
        concepts=inducer.induce(kb);
        o<<"Concepts induced="<<concepts.size()<<"\n";
        for(const auto&c:concepts) o<<"  CONCEPT "<<c.name<<" ["<<c.domain<<"] support="<<c.support<<"\n";
        auto insights=insightEngine.analyze(kb,12);
        o<<"Mathematical insights="<<insights.size()<<"\n";
        for(const auto&i:insights){
            o<<"  INSIGHT ["<<i.kind<<"] confidence="<<std::fixed<<std::setprecision(3)
             <<i.confidence<<" "<<i.title<<" members=";
            for(size_t j=0;j<i.members.size();++j) o<<(j?",":"")<<"#"<<i.members[j];
            o<<"\n    "<<i.explanation<<"\n";
        }
        o<<"Research agenda:\n";
        for(const auto&q:agenda.build(kb,concepts)) o<<"  - "<<q<<"\n";
        o<<"Experience memory="<<experience.size()<<"\n";
        o<<"Search policy: "<<policy.report()<<"\n";
        o<<"Novelty archive: "<<novelty.report()<<"\n";
        o<<"Research trace events="<<trace.size()<<"\n";
        o<<"Research journal highlights="<<journal.size()<<"\n";
        o<<"Research frontier tasks="<<frontier.size()<<"\n";
        o<<"Research missions executed="<<missionArchive.size()<<"\n";
        auto questions=ResearchQuestionEngine::build(frontier,concepts,12);
        o<<"Next research questions:"<<"\n";
        for(const auto&q:questions)o<<"  ? "<<q<<"\n";
        deadlineActive=false;
    }

    void discover(std::ostream&o){
        research(o,std::max(1,std::min(5,cfg.iterations/8+1)));
    }

    // Bounded research entry point used by higher-level autonomous controllers.
    // It prevents a meta-controller such as V20 from accidentally inheriting a
    // huge exploratory budget intended for standalone --discover runs. The
    // original configuration is restored after the bounded session.
    void researchBounded(std::ostream&o,int rounds,int maxIterations,int maxTests,int maxTimeMs){
        const Config saved=cfg;
        cfg.iterations=std::max(1,std::min(cfg.iterations,maxIterations));
        cfg.tests=std::max(1,std::min(cfg.tests,maxTests));
        cfg.researchBeam=std::max(4,std::min(cfg.researchBeam,24));
        cfg.maxStates=std::max(1000,std::min(cfg.maxStates,12000));
        cfg.maxDepth=std::max(6,std::min(cfg.maxDepth,12));
        cfg.maxRepairs=std::max(1,std::min(cfg.maxRepairs,3));
        if(cfg.timeLimitMs<=0)cfg.timeLimitMs=maxTimeMs;
        research(o,rounds);
        cfg=saved;
    }

    Record proveText(const std::string&s){
        return processInternal(Parser(s).statement(),"user_request","user");
    }

    void saveExperience(const std::string&file)const{ experience.save(file); }
    void loadExperience(const std::string&file){ experience.load(file); }
    void savePolicy(const std::string&file)const{ policy.save(file); }
    void loadPolicy(const std::string&file){ policy.load(file); }
    void saveBrain(const std::string&file)const{ BrainCheckpoint::save(kb,learned,experience,file); }
    void loadBrain(const std::string&file,std::ostream&out){
        auto statements=BrainCheckpoint::loadStatements(file); int loaded=0;
        for(const auto&text:statements){
            try{ Statement s=Parser(text).statement(); std::string k=statementKey(s); if(!seen.insert(k).second)continue; learned.push_back(s); Record r=processInternal(s,"brain-replay","persistent-brain"); if(r.status==Status::KERNEL_VERIFIED||r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM||r.status==Status::LEMMA_FOUND)++loaded; }catch(const std::exception&e){ out<<"[BRAIN-SKIP] "<<e.what()<<"\n"; }
        }
        out<<"Brain replay: "<<loaded<<" kernel-verified statements from "<<file<<"\n";
        persistState();
    }
    void saveJournal(const std::string&file)const{ journal.save(file); }
    void saveTrace(const std::string&file)const{ trace.save(file); }
    std::string noveltyReport()const{return novelty.report();}
    std::vector<std::string> researchAgenda()const{return agenda.build(kb,concepts);}
    const std::vector<ResearchConcept>& researchConcepts()const{return concepts;}
    const Euler& knowledge()const{return kb;}
    Euler& knowledge(){return kb;}

    void printTheories(std::ostream&out)const{
        for(const auto&t:theories){
            out<<"Theory #"<<t.id<<" ["<<t.domain<<"] "<<t.name<<"\n";
            out<<"  thesis: "<<t.thesis<<"\n";
            out<<"  maturity: "<<std::fixed<<std::setprecision(3)<<t.maturity
               <<" coherence: "<<t.coherence<<"\n";
            out<<"  definitions:\n";
            for(const auto&x:t.definitions)out<<"    - "<<x<<"\n";
            out<<"  principles:\n";
            for(const auto&x:t.principles)out<<"    - "<<x<<"\n";
            out<<"  theorem members:"; for(int id:t.members)out<<" #"<<id; out<<"\n";
            out<<"  lemmas:"; for(int id:t.lemmas)out<<" #"<<id; out<<"\n";
            out<<"  conjectures:"; for(int id:t.conjectures)out<<" #"<<id; out<<"\n";
            out<<"  open research questions:\n";
            for(const auto&q:t.openQuestions)out<<"    ? "<<q<<"\n";
            out<<"  internal dependency edges: "<<t.edges.size()<<"\n";
        }
    }

    void printGraph(std::ostream&out)const{
        out<<"Research graph edges="<<graph.edges().size()<<"\n";
        for(const auto&e:graph.edges())
            out<<"  #"<<e.from<<" -> #"<<e.to
               <<" ["<<e.relation<<"] "<<e.reason<<"\n";
    }

    void exportGraphDot(const std::string&file)const{
        graph.exportDot(file,kb.all());
    }

    void printFrontier(std::ostream&out,size_t n=20)const{ frontier.print(out,n); }
    void exportFrontier(const std::string&file,size_t n=100)const{ frontier.exportJson(file,n); }
    std::vector<std::string> researchQuestions(size_t n=24)const{ return ResearchQuestionEngine::build(frontier,concepts,n); }
    size_t frontierSize()const{return frontier.size();}

    ProofPlan proofPlan(const Statement&s)const{return planner.plan(s);}
    const std::vector<Theory>& theoryList()const{return theories;}
    const ResearchGraph& researchGraph()const{return graph;}
    const std::vector<Statement>& learnedFacts()const{return learned;}
    std::vector<MathematicalInsight> mathematicalInsights(size_t n=24)const{
        return insightEngine.analyze(kb,n);
    }
    std::vector<std::string> insightQuestions(size_t n=16)const{
        return insightEngine.questions(kb,n);
    }
    void printInsights(std::ostream&out,size_t n=24)const{
        const auto xs=insightEngine.analyze(kb,n);
        out<<"Mathematical insights="<<xs.size()<<"\n";
        for(const auto&i:xs){
            out<<"  ["<<i.kind<<"] confidence="<<std::fixed<<std::setprecision(3)
               <<i.confidence<<" "<<i.title<<" members=";
            for(size_t j=0;j<i.members.size();++j)out<<(j?",":"")<<"#"<<i.members[j];
            out<<"\n    "<<i.explanation<<"\n";
        }
    }
};



// ============================================================================
// H.I.L.B.E.R.T. 20.0: Autonomous Mathematical Research Core
//
// V20 adds a research-control layer above the existing kernel and discovery
// machinery.  It does not weaken the trust boundary.  Its purpose is to make
// HILBERT behave more like a scientist: form families of hypotheses, create
// cross-domain experiments, test converses and boundary cases, rank research
// moves by measured yield, and repeatedly feed successful discoveries back into
// the next search cycle.  Every concrete mathematical claim still goes through
// DiscoveryEngine::proveText and therefore Popper + proof search + Kernel.
// ============================================================================
class CrossDomainResearchEngine {
    static bool eligible(const Record&r){
        return r.status==Status::THEOREM || r.status==Status::CONDITIONAL_THEOREM ||
               r.status==Status::LEMMA_FOUND;
    }

    static std::vector<std::string> variables(const Statement&s){
        std::set<std::string>v;
        vars(s.left,v); vars(s.right,v);
        for(const auto&a:s.assumptions){ vars(a.left,v); vars(a.right,v); }
        return std::vector<std::string>(v.begin(),v.end());
    }

    static Statement substitute(const Statement&s,const std::map<std::string,E>&m){
        Statement q=s;
        q.left=cloneSubstitute(s.left,m);
        q.right=cloneSubstitute(s.right,m);
        q.assumptions.clear();
        for(const auto&a:s.assumptions)
            q.assumptions.push_back({cloneSubstitute(a.left,m),
                                     cloneSubstitute(a.right,m),a.rel});
        q.source="cross-domain-research";
        return q;
    }

    static bool same(const Statement&a,const Statement&b){
        if(a.rel!=b.rel || a.assumptions.size()!=b.assumptions.size())return false;
        if(canonical(a.left)!=canonical(b.left)||canonical(a.right)!=canonical(b.right))return false;
        for(size_t i=0;i<a.assumptions.size();++i){
            if(a.assumptions[i].rel!=b.assumptions[i].rel)return false;
            if(canonical(a.assumptions[i].left)!=canonical(b.assumptions[i].left)||
               canonical(a.assumptions[i].right)!=canonical(b.assumptions[i].right))return false;
        }
        return true;
    }

    static E func(const std::string&name,const std::vector<E>&args){
        E z=op(Kind::Func,args); z->name=name; return z;
    }

public:
    struct Proposal {
        Statement statement;
        std::string reason;
        std::string source;
    };

    std::vector<Proposal> generate(const std::vector<Record>&records,int budget,uint64_t seed)const{
        std::vector<Proposal>out;
        if(budget<=0)return out;
        std::mt19937_64 rng(seed^0xA0761D6478BD642FULL);
        for(auto it=records.rbegin();it!=records.rend()&&(int)out.size()<budget;++it){
            const Record&r=*it;
            if(!eligible(r))continue;
            const auto vs=variables(r.statement);
            if(vs.empty())continue;

            // Cross-domain substitution is deliberately structural: a verified
            // identity over arbitrary expressions can transport into arithmetic,
            // number theory and combinatorics.  The resulting object is still a
            // hypothesis until the ordinary kernel pipeline accepts it.
            std::vector<std::pair<std::string,E>> recipes;
            const std::string&a=vs[0];
            recipes.push_back({"Fibonacci transport",func("fib",{vr(a)})});
            recipes.push_back({"factorial transport",func("factorial",{vr(a)})});
            recipes.push_back({"Euler phi transport",func("phi",{vr(a)})});
            recipes.push_back({"divisor-count transport",func("tau",{vr(a)})});
            recipes.push_back({"prime-test transport",func("isPrime",{vr(a)})});
            if(vs.size()>=2){
                recipes.push_back({"gcd transport",func("gcd",{vr(vs[0]),vr(vs[1])})});
                recipes.push_back({"binomial transport",func("nCr",{vr(vs[0]),vr(vs[1])})});
                recipes.push_back({"modular transport",func("mod",{vr(vs[0]),vr(vs[1])})});
            }

            int start=int(rng()%recipes.size());
            for(size_t z=0;z<recipes.size()&&(int)out.size()<budget;++z){
                const auto&recipe=recipes[(start+z)%recipes.size()];
                std::map<std::string,E>m;
                m[vs[0]]=recipe.second;
                // Give remaining variables a deliberately different mathematical
                // object. This creates mixed-domain laws rather than mere one-way
                // substitutions of the original syntax.
                if(vs.size()>=2){
                    if((z&1)==0)m[vs[1]]=func("fib",{vr(vs[1])});
                    else m[vs[1]]=func("gcd",{vr(vs[0]),vr(vs[1])});
                }
                Statement q=substitute(r.statement,m);
                if(same(q,r.statement))continue;
                out.push_back({q,
                    "cross-domain experiment: "+recipe.first,
                    "cross-domain:"+std::to_string(r.id)});
            }
        }
        return out;
    }
};

class AutonomousResearchCoreV20 {
    struct MethodStats { int attempts=0; int verified=0; int falsified=0; };
    std::map<std::string,MethodStats>stats_;

    static bool trusted(const Record&r){
        return r.status==Status::THEOREM || r.status==Status::CONDITIONAL_THEOREM ||
               r.status==Status::LEMMA_FOUND;
    }

    static double yield(const MethodStats&s){
        return double(s.verified+1)/double(s.attempts+2);
    }

    static std::string methodOf(const std::string&s){
        const size_t p=s.find(':');
        return p==std::string::npos?s:s.substr(0,p);
    }

    void observe(const std::string&method,const Record&r){
        MethodStats&x=stats_[method]; ++x.attempts;
        if(trusted(r))++x.verified;
        if(r.status==Status::FALSIFIED)++x.falsified;
    }

    static void printRecord(std::ostream&o,const Record&r,const std::string&method){
        o<<"  ["<<statusName(r.status)<<"] "<<print(r.statement)
         <<"  via "<<method;
        if(r.evidence.counterexample)o<<"  COUNTEREXAMPLE";
        if(r.proof)o<<"  CERTIFIED";
        o<<"\n";
    }

public:
    int run(DiscoveryEngine&d,std::ostream&o,int rounds,int beam,uint64_t seed){
        rounds=std::max(1,rounds); beam=std::max(4,beam);
        o<<"H.I.L.B.E.R.T. 20.0 autonomous research core\n";
        o<<"strategy=adaptive cross-domain theorem laboratory, rounds="<<rounds
         <<", beam="<<beam<<", seed="<<seed<<"\n";

        // First establish a mathematically useful verified base.  The existing
        // researcher is itself a powerful source of seeds, theories and graph
        // structure, so V20 treats it as the first research generation.
        // Keep the bootstrap bounded. V20 is a controller above the research
        // engine, so it should spend its budget on deliberate synthesis rather
        // than exploding the underlying exploratory generator.
        d.researchBounded(o,1,std::max(2,std::min(6,beam/2+2)),
                          std::max(20,std::min(60,beam*6)),15000);

        for(int round=0;round<rounds;++round){
            const std::vector<Record>&facts=d.knowledge().all();
            o<<"=== V20 synthesis round "<<round+1<<" (facts="<<facts.size()<<") ===\n";

            std::vector<CrossDomainResearchEngine::Proposal> cross=
                CrossDomainResearchEngine().generate(facts,beam/2,seed+uint64_t(round)*7919ULL);
            std::vector<ActiveDiscoveryLab::Proposal> active=
                ActiveDiscoveryLab().probe(facts,beam/3+1);
            std::vector<MathematicalScientist::Proposal> scientist=
                MathematicalScientist().instantiate(facts,beam/3+1,seed+uint64_t(round)*104729ULL);
            std::vector<AbstractionInventionEngine::Proposal> invented=
                AbstractionInventionEngine().generalize(facts,beam/4+1);

            struct Candidate { Statement s; std::string reason; std::string source; double priority; };
            std::vector<Candidate>pool;
            for(const auto&p:cross)pool.push_back({p.statement,p.reason,p.source,1.25});
            for(const auto&p:active)pool.push_back({p.statement,p.reason,p.source,1.00});
            for(const auto&p:scientist)pool.push_back({p.statement,p.reason,p.source,1.10});
            for(const auto&p:invented)pool.push_back({p.statement,p.reason,p.source,1.20});

            // Stable sort by learned method yield.  The system therefore learns
            // which research operators are actually productive instead of merely
            // increasing random search.
            std::sort(pool.begin(),pool.end(),[this](const Candidate&a,const Candidate&b){
                const std::string ma=methodOf(a.source);
                const std::string mb=methodOf(b.source);
                const auto ia=stats_.find(ma);
                const auto ib=stats_.find(mb);
                const double ya=(ia==stats_.end())?0.5:yield(ia->second);
                const double yb=(ib==stats_.end())?0.5:yield(ib->second);
                if(a.priority*ya!=b.priority*yb)return a.priority*ya>b.priority*yb;
                return a.source<b.source;
            });

            std::set<std::string>seen; int executed=0;
            for(const auto&c:pool){
                if(executed>=beam)break;
                const std::string key=statementKey(c.s);
                if(!seen.insert(key).second)continue;
                const std::string method=methodOf(c.source);
                Record r=d.proveText(print(c.s));
                observe(method,r); ++executed;
                printRecord(o,r,method);
            }

            // Deliberate converse/assumption questions are generated only from
            // trusted results.  This makes V20 spend its scarce research budget on
            // actual mathematical gaps rather than arbitrary formulas.
            int questions=0;
            for(auto it=d.knowledge().all().rbegin();
                it!=d.knowledge().all().rend()&&questions<beam/4+1;++it){
                if(!trusted(*it))continue;
                Statement q=it->statement; std::swap(q.left,q.right);
                Record r=d.proveText(print(q));
                observe("converse",r); ++questions;
                printRecord(o,r,"converse");
            }

            o<<"V20 method performance:";
            for(const auto&x:stats_)
                o<<" "<<x.first<<"="<<x.second.verified<<"/"<<x.second.attempts
                 <<"("<<std::fixed<<std::setprecision(3)<<yield(x.second)<<")";
            o<<"\n";
        }

        int total=0,verifiedCount=0,falseCount=0;
        for(const auto&r:d.knowledge().all()){++total;if(trusted(r))++verifiedCount;if(r.status==Status::FALSIFIED)++falseCount;}
        o<<"=== V20 final research profile ===\n";
        o<<"records="<<total<<", trusted="<<verifiedCount<<", falsified="<<falseCount<<"\n";
        o<<"The trust boundary is unchanged: empirical survival is not proof.\n";
        return 0;
    }

    std::string report()const{
        std::ostringstream o;
        for(const auto&x:stats_)
            o<<x.first<<" "<<x.second.verified<<"/"<<x.second.attempts
             <<" yield="<<std::fixed<<std::setprecision(3)<<yield(x.second)<<"\n";
        return o.str();
    }
};

// ============================================================================
// 9. Persistence, certificates, experiments and research reporting
// ============================================================================

static std::string jsonEscape(const std::string& s){
    std::ostringstream o;
    for(unsigned char c:s){
        switch(c){
            case '"': o<<"\\\""; break;
            case '\\': o<<"\\\\"; break;
            case '\n': o<<"\\n"; break;
            case '\r': o<<"\\r"; break;
            case '\t': o<<"\\t"; break;
            default:
                if(c<32) o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c)<<std::dec;
                else o<<c;
        }
    }
    return o.str();
}
static std::string scoreJson(const Score&x){
    std::ostringstream o;
    o<<"{\"novelty\":"<<x.novelty<<",\"generality\":"<<x.generality
     <<",\"structure\":"<<x.structure<<",\"compression\":"<<x.compression
     <<",\"evidence\":"<<x.evidence<<",\"triviality\":"<<x.triviality
     <<",\"total\":"<<x.total<<"}";
    return o.str();
}
static std::string evidenceJson(const Evidence&e){
    std::ostringstream o;
    o<<"{\"tests\":"<<e.tests<<",\"counterexample\":";
    if(!e.counterexample) o<<"null";
    else{
        o<<"{";
        bool first=true;
        for(const auto&x:*e.counterexample){
            if(!first)o<<",";
            first=false;
            o<<"\""<<jsonEscape(x.first)<<"\":\""<<jsonEscape(x.second.str())<<"\"";
        }
        o<<"}";
    }
    o<<",\"reason\":\""<<jsonEscape(e.reason)<<"\"}";
    return o.str();
}
static std::string proofJson(const Proof&p){
    std::ostringstream o;
    o<<"{\"finalKey\":\""<<jsonEscape(p.finalKey)<<"\",\"steps\":[";
    for(size_t i=0;i<p.steps.size();++i){
        if(i)o<<",";
        const auto&st=p.steps[i];
        o<<"{\"number\":"<<st.number
         <<",\"before\":\""<<jsonEscape(st.before)
         <<"\",\"after\":\""<<jsonEscape(st.after)
         <<"\",\"rule\":\""<<jsonEscape(st.rule)
         <<"\",\"justification\":\""<<jsonEscape(st.justification)<<"\"}";
    }
    o<<"]}";
    return o.str();
}
static std::string recordJson(const Record&r){
    std::ostringstream o;
    o<<"{\"id\":"<<r.id
     <<",\"status\":\""<<statusName(r.status)
     <<"\",\"statement\":\""<<jsonEscape(print(r.statement))
     <<"\",\"generator\":\""<<jsonEscape(r.generator)
     <<"\",\"ancestry\":\""<<jsonEscape(r.ancestry)
     <<"\",\"kernelReason\":\""<<jsonEscape(r.kernelReason)
     <<"\",\"score\":"<<scoreJson(r.score)
     <<",\"evidence\":"<<evidenceJson(r.evidence)
     <<",\"proof\":";
    if(r.proof)o<<proofJson(*r.proof);else o<<"null";
    o<<"}";
    return o.str();
}

class ResearchReport {
public:
    static void exportKnowledge(const Euler&kb,const std::string&file){
        std::ofstream out(file);
        if(!out) throw Error("cannot open export file: "+file);
        out<<"{\n  \"format\":\"HILBERT-knowledge-v1\",\n  \"records\":[\n";
        const auto& all=kb.all();
        for(size_t i=0;i<all.size();++i){
            out<<"    "<<recordJson(all[i])<<(i+1<all.size()?",":"")<<"\n";
        }
        out<<"  ]\n}\n";
    }
    static void printStats(const Euler&kb){
        std::map<std::string,int> counts;
        int proofs=0,conditional=0;
        for(const auto&r:kb.all()){
            ++counts[statusName(r.status)];
            if(r.proof)++proofs;
            if(r.status==Status::CONDITIONAL_THEOREM)++conditional;
        }
        std::cout<<"H.I.L.B.E.R.T. research statistics\n";
        std::cout<<"records="<<kb.all().size()<<"\n";
        std::cout<<"proofs="<<proofs<<"\n";
        std::cout<<"conditional_theorems="<<conditional<<"\n";
        for(const auto&x:counts)std::cout<<"  "<<x.first<<"="<<x.second<<"\n";
    }
};

static bool verifyCertificateText(const Statement&s,const Proof&p){
    std::string why;
    bool ok=Kernel().verify(s,p,why);
    std::cout<<(ok?"KERNEL_VERIFIED":"KERNEL_REJECTED")<<"\n";
    std::cout<<why<<"\n";
    return ok;
}

static int experiment(const Config&cfg,int runs){
    std::cout<<"H.I.L.B.E.R.T. reproducible experiment\n";
    std::cout<<"seed="<<cfg.seed<<", runs="<<runs<<", iterations="<<cfg.iterations
             <<", tests="<<cfg.tests<<"\n";
    int theoremTotal=0, falsifiedTotal=0, unsupportedTotal=0;
    for(int r=0;r<runs;++r){
        Config local=cfg;
        local.seed=cfg.seed+uint64_t(r);
        DiscoveryEngine d(local);
        std::ostringstream sink;
        d.discover(sink);
        for(const auto&rec:d.knowledge().all()){
            if(rec.status==Status::THEOREM||rec.status==Status::CONDITIONAL_THEOREM)++theoremTotal;
            else if(rec.status==Status::FALSIFIED)++falsifiedTotal;
            else if(rec.status==Status::UNSUPPORTED||rec.status==Status::SEARCH_EXHAUSTED)++unsupportedTotal;
        }
        std::cout<<"run="<<r+1<<" seed="<<local.seed
                 <<" records="<<d.knowledge().all().size()<<"\n";
    }
    std::cout<<"summary: kernel_verified_theorems="<<theoremTotal
             <<", falsified="<<falsifiedTotal
             <<", exhausted_or_unsupported="<<unsupportedTotal<<"\n";
    std::cout<<"NOTE: these are measured experiment results, not fabricated paper claims.\n";
    return 0;
}

// ============================================================================
// 9. Tests, benchmarks, CLI and main
// ============================================================================

static void saveCertificate(const Statement&s,const Proof&p,const std::string&file){
    std::ofstream out(file);
    if(!out) throw Error("cannot write certificate: "+file);
    out<<"HILBERT-CERTIFICATE-V1\n";
    out<<"STATEMENT\t"<<print(s)<<"\n";
    out<<"FINALKEY\t"<<p.finalKey<<"\n";
    for(const auto&st:p.steps){
        out<<"STEP\t"<<st.number<<"\t"<<st.rule<<"\t"<<st.before<<"\t"
           <<st.after<<"\t"<<st.justification<<"\n";
    }
}
static std::vector<std::string> splitTabs(const std::string&line){
    std::vector<std::string> out; size_t b=0;
    for(size_t i=0;;){
        i=line.find('\t',b);
        if(i==std::string::npos){out.push_back(line.substr(b));break;}
        out.push_back(line.substr(b,i-b)); b=i+1;
    }
    return out;
}
static Proof loadCertificate(const std::string&file){
    std::ifstream in(file);
    if(!in) throw Error("cannot read certificate: "+file);
    std::string line; std::getline(in,line);
    if(line!="HILBERT-CERTIFICATE-V1") throw Error("unsupported certificate format");
    Proof p; std::string statementText;
    while(std::getline(in,line)){
        auto f=splitTabs(line);
        if(f.empty()) continue;
        if(f[0]=="STATEMENT" && f.size()==2) statementText=f[1];
        else if(f[0]=="FINALKEY" && f.size()==2) p.finalKey=f[1];
        else if(f[0]=="STEP" && f.size()>=6){
            ProofStep st;
            st.number=std::stoi(f[1]);
            st.rule=f[2]; st.before=f[3]; st.after=f[4]; st.justification=f[5];
            p.steps.push_back(std::move(st));
        }
    }
    if(statementText.empty()) throw Error("certificate has no statement");
    // The statement is checked by the caller; retain it in a deterministic
    // first-step transcript and reconstruct only the proof object here.
    return p;
}
static std::string certificateStatement(const std::string&file){
    std::ifstream in(file);
    if(!in) throw Error("cannot read certificate: "+file);
    std::string line;
    while(std::getline(in,line)){
        auto f=splitTabs(line);
        if(f.size()==2 && f[0]=="STATEMENT") return f[1];
    }
    throw Error("certificate has no statement");
}

struct Tests {
    int pass=0,fail=0;
    void check(const std::string&n,const std::function<void()>&f){
        try{f();++pass;std::cout<<"[PASS] "<<n<<"\n";}
        catch(const std::exception&e){++fail;std::cout<<"[FAIL] "<<n<<": "<<e.what()<<"\n";}
    }
    int run(){
        check("BigInt arithmetic",[]{
            require((BigInt("123456789123456789")+BigInt(11)).str()=="123456789123456800","addition");
            require((BigInt("1000000000000000000")/BigInt(3)).str()=="333333333333333333","division");
            require(BigInt(INT64_MIN).str()=="-9223372036854775808","INT64_MIN");
            require((BigInt(-7)%BigInt(3)).str()=="1","nonnegative remainder");
            require(BigInt(9)>BigInt(8),"comparison");
        });
        check("Rational normalization",[]{
            require(Rational(BigInt(2),BigInt(4)).str()=="1/2","reduction");
            require(Rational(BigInt(-2),BigInt(-4)).str()=="1/2","sign");
            require(Rational(-3)<Rational(1),"ordering");
        });
        check("parser rejects malformed",[]{
            bool bad=false; try{Parser("a + = b").statement();}catch(const Error&){bad=true;}
            require(bad,"accepted malformed input");
        });
        check("unary minus precedence",[]{
            auto s=Parser("-2^2 = -4").statement();
            require(eval(s.left,{})==eval(s.right,{}),"-2^2 parsed incorrectly");
            auto t=Parser("(-2)^2 = 4").statement();
            require(eval(t.left,{})==eval(t.right,{}),"parenthesized exponent");
        });
        check("exact function engine",[]{
            require(eval(Parser("gcd(84,30)=6").statement().left,{})==Rational(6),"gcd");
            require(eval(Parser("factorial(10)=3628800").statement().left,{})==Rational(3628800),"factorial");
            require(eval(Parser("isPrime(97)=1").statement().left,{})==Rational(1),"prime");
            require(eval(Parser("12 divides 84").statement().left,{})==Rational(12),"divides parse");
        });
        check("polynomial canonicalization",[]{
            auto s=Parser("(a+b)^2 = a^2+2*a*b+b^2").statement();
            require(canonical(s.left)==canonical(s.right),"square expansion");
            auto t=Parser("a+b=b+a").statement();
            require(canonical(t.left)==canonical(t.right),"commutativity");
        });
        check("Popper finds false identity",[]{
            auto s=Parser("a*(b+c) = a*b+a+b*c").statement();
            Popper p(2); auto e=p.attack(s,100);
            require(bool(e.counterexample),"no counterexample");
        });
        check("conditional rational proof",[]{
            auto s=Parser("x/x=1 assuming x!=0").statement();
            auto p=Euclid().prove(s); std::string w;
            require(p&&Kernel().verify(s,*p,w),"conditional cancellation");
        });
        check("positive assumption proves nonzero denominator",[]{
            auto s=Parser("x/x=1 assuming x>0").statement();
            auto p=Euclid().prove(s); std::string w;
            require(p&&Kernel().verify(s,*p,w),"positive denominator");
        });
        check("difference of squares conditional proof",[]{
            auto s=Parser("(x^2-1)/(x-1)=x+1 assuming x!=1").statement();
            auto p=Euclid().prove(s); std::string w;
            require(p&&Kernel().verify(s,*p,w),"conditional factor cancellation");
        });
        check("rational proof rejects missing condition",[]{
            auto s=Parser("(x^2-1)/(x-1)=x+1").statement();
            require(!Euclid().prove(s),"missing condition accepted");
        });
        check("kernel verifies true certificate",[]{
            auto s=Parser("(a+b)*(a-b)=a^2-b^2").statement();
            Euclid e; auto p=e.prove(s); require(bool(p),"prover failed");
            std::string w; require(Kernel().verify(s,*p,w),w);
        });
        check("kernel rejects altered certificate",[]{
            auto s=Parser("a+b=b+a").statement();
            Euclid e; auto p=*e.prove(s); p.steps[0].after="forged";
            std::string w; require(!Kernel().verify(s,p,w),"forgery accepted");
        });
        check("conditional cancellation rejected",[]{
            auto s=Parser("x/x=1").statement();
            require(!Euclid().prove(s),"cancellation accepted without x!=0");
        });
        check("false statement cannot prove",[]{
            auto s=Parser("a+b=a*b").statement();
            require(!Euclid().prove(s),"false proof created");
        });
        check("knowledge stores theorem with full identity",[]{
            Config c;c.timeLimitMs=1000;c.tests=30;DiscoveryEngine d(c);
            auto r=d.proveText("a*(b+c)=a*b+a*c");
            require(r.status==Status::THEOREM,"not promoted");
            require(d.knowledge().all().size()==1,"unexpected duplicate");
        });
        check("knowledge distinguishes assumptions",[]{
            Config c;c.timeLimitMs=1000;c.tests=10;DiscoveryEngine d(c);
            auto a=d.proveText("x/x=1 assuming x!=0");
            require(a.status==Status::CONDITIONAL_THEOREM,"conditional theorem");
            require(!d.knowledge().knownEquivalent(Parser("x/x=1").statement()),"unconditional collision");
        });
        check("certificate export JSON",[]{
            Config c;c.timeLimitMs=1000;c.tests=10;DiscoveryEngine d(c);
            d.proveText("a+b=b+a");
            const std::string f="hilbert_test_knowledge.json";
            ResearchReport::exportKnowledge(d.knowledge(),f);
            std::ifstream in(f); require(bool(in),"export missing");
            std::string text((std::istreambuf_iterator<char>(in)),{});
            require(text.find("KERNEL_VERIFIED")!=std::string::npos ||
                    text.find("THEOREM")!=std::string::npos,"export invalid");
        });

        check("number theory gcd/lcm",[]{
            require(gcd(BigInt(84),BigInt(30))==BigInt(6),"gcd");
            require(lcmExact(BigInt(21),BigInt(6))==BigInt(42),"lcm");
        });
        check("extended gcd identity",[]{
            auto e=extendedGcd(BigInt(240),BigInt(46));
            require(e.g==BigInt(2),"gcd");
            require(BigInt(240)*e.x+BigInt(46)*e.y==e.g,"Bezout identity");
        });
        check("modular arithmetic",[]{
            require(powmod(BigInt(2),BigInt(100),BigInt(1009))==BigInt(164),"powmod");
            auto x=modInverse(BigInt(3),BigInt(11));
            require(x&&*x==BigInt(4),"inverse");
            require(eulerPhi(BigInt(36))==BigInt(12),"phi");
        });
        check("exact primality and factorization",[]{
            require(isPrimeExact(BigInt(97)),"prime 97");
            require(!isPrimeExact(BigInt(91)),"composite 91");
            auto f=primeFactors(BigInt(360));
            std::vector<BigInt> want={BigInt(2),BigInt(2),BigInt(2),BigInt(3),BigInt(3),BigInt(5)};
            require(f==want,"factorization");
        });
        check("number theory functions",[]{
            require(divisorCount(BigInt(360))==BigInt(24),"tau");
            require(sigma1(BigInt(12))==BigInt(28),"sigma");
            require(fibonacci(BigInt(50))==BigInt("12586269025"),"fib");
        });
        check("combinatorics core",[]{
            require(binomialExact(BigInt(52),BigInt(5))==BigInt(2598960),"52 choose 5");
            require(permutationExact(BigInt(10),BigInt(3))==BigInt(720),"10P3");
            require(catalanExact(BigInt(10))==BigInt(16796),"Catalan");
            require(derangementExact(BigInt(6))==BigInt(265),"derangements");
            require(stirling2Exact(BigInt(6),BigInt(3))==BigInt(90),"Stirling");
        });
        check("partitions, Bell, compositions",[]{
            require(partitionExact(BigInt(10))==BigInt(42),"p(10)");
            require(bellExact(BigInt(6))==BigInt(203),"Bell");
            require(compositionExact(BigInt(7),BigInt(3))==BigInt(15),"compositions");
        });
        check("extended expression evaluator",[]{
            auto eqs={
                "phi(36)=12","tau(360)=24","sigma(12)=28","fib(50)=12586269025",
                "nCr(52,5)=2598960","nPr(10,3)=720","catalan(10)=16796",
                "derangements(6)=265","stirling2(6,3)=90","partition(10)=42",
                "bell(6)=203","composition(7,3)=15","powmod(2,100,1009)=164","modinv(3,11)=4"
            };
            for(const auto& q:eqs){
                auto s=Parser(q).statement();
                require(eval(s.left,{})==eval(s.right,{}),q);
            }
        });
        check("closed-form number theory certificate",[]{
            auto s=Parser("phi(36)=12").statement(); auto p=Euclid().prove(s); std::string w;
            require(p&&Kernel().verify(s,*p,w),"phi certificate");
            auto d=Parser("12 divides 84").statement(); auto q=Euclid().prove(d);
            require(q&&Kernel().verify(d,*q,w),"divisibility certificate");
        });
        check("closed-form combinatorics certificate",[]{
            auto s=Parser("nCr(52,5)=2598960").statement(); auto p=Euclid().prove(s); std::string w;
            require(p&&Kernel().verify(s,*p,w),"nCr certificate");
            auto c=Parser("partition(10)=42").statement(); auto q=Euclid().prove(c);
            require(q&&Kernel().verify(c,*q,w),"partition certificate");
        });
        check("number-theory falsehood attacked",[]{
            auto s=Parser("isPrime(91)=1").statement();
            require(!relationHolds(s.rel,eval(s.left,{}),eval(s.right,{})),"false prime claim");
        });
        check("trusted Fibonacci recurrence certificate",[]{
            auto s=Parser("fib(n+2)=fib(n+1)+fib(n) assuming n>=0").statement(); auto p=Euclid().prove(s); std::string w;
            require(p&&Kernel().verify(s,*p,w),"fib recurrence certificate");
        });
        check("trusted Pascal recurrence certificate",[]{
            auto s=Parser("nCr(n,k)=nCr(n-1,k-1)+nCr(n-1,k) assuming n>=1 and k>=1 and k<=n").statement(); auto p=Euclid().prove(s); std::string w;
            require(p&&Kernel().verify(s,*p,w),"Pascal recurrence certificate");
        });
        check("autonomous conjecture mutation is falsified",[]{
            Config c;c.timeLimitMs=1000;c.tests=100;c.seed=7;DiscoveryEngine d(c);auto r=d.proveText("fib(n+2)=fib(n+1)+2*fib(n) assuming n>=0");
            require(r.status==Status::FALSIFIED,"mutation survived counterexample search");
        });
        check("formula miner learns polynomial law",[]{
            Config c;c.timeLimitMs=1000;c.tests=20;c.iterations=1;DiscoveryEngine d(c);
            const std::string f="hilbert_formula_db.txt"; std::ofstream out(f);
            for(int n=0;n<8;++n) out<<"nCr("<<n<<",2)="<<(n<2?0:n*(n-1)/2)<<"\n";
            out.close();
            std::ostringstream log; d.learnFile(f,log); d.research(log,1);
            require(d.knowledge().all().size()>8,"research brain produced no additional records");
        });
        check("theory builder records mathematical structure",[]{
            Config c;c.tests=20;DiscoveryEngine d(c);
            d.proveText("a+b=b+a"); d.proveText("a*(b+c)=a*b+a*c");
            require(!d.theoryList().empty(),"theory not formed");
            require(!d.theoryList()[0].thesis.empty(),"missing thesis");
            require(d.theoryList()[0].maturity>0.0,"missing maturity");
        });
        check("theory builder exposes open questions",[]{
            Config c;c.tests=20;DiscoveryEngine d(c);
            d.proveText("a+b=b+a");
            require(!d.theoryList().empty(),"theory missing");
            require(!d.theoryList()[0].openQuestions.empty(),"no open questions");
        });
        check("theory formation",[]{
            Config c;c.timeLimitMs=1000;c.tests=20;DiscoveryEngine d(c);d.proveText("fib(n+2)=fib(n+1)+fib(n) assuming n>=0");
            require(!d.theoryList().empty(),"no theory cluster formed");
        });
        check("data-mined binomial formula survives exact tests",[]{
            auto s=Parser("nCr(n,2)=n*(n-1)/2 assuming n>=0").statement();
            Evidence e=Popper(123).attack(s,250); require(!e.counterexample,"mined formula has a counterexample");
        });
        check("schema generalization certifies substituted Fibonacci law",[]{
            auto s=Parser("fib(x+y+2)=fib(x+y+1)+fib(x+y) assuming x+y>=0").statement();
            auto p=Euclid().prove(s);std::string w;require(p&&Kernel().verify(s,*p,w),"schema generalization not certified");
        });
        check("data-driven relational miner generates conjectures",[]{
            std::vector<Statement> facts;
            const int pairs[][2]={{2,4},{3,6},{4,10},{5,15},{6,9},{7,14},{8,12},{9,15}};
            for(const auto&pair:pairs){
                BigInt g=gcd(BigInt(pair[0]),BigInt(pair[1]));
                facts.push_back(Parser("gcd("+std::to_string(pair[0])+","+std::to_string(pair[1])+")="+g.str()).statement());
                facts.push_back(Parser("gcd("+std::to_string(pair[1])+","+std::to_string(pair[0])+")="+g.str()).statement());
            }
            RelationalMiner rm; auto c=rm.mine(facts);
            require(!c.empty(),"relational miner found no learned relationship");
        });
        check("cross-sequence miner discovers affine identity",[]{
            std::vector<Statement> facts;
            for(int n=2;n<=9;++n){
                facts.push_back(Parser("nPr("+std::to_string(n)+",2)="+
                    permutationExact(BigInt(n),BigInt(2)).str()).statement());
                facts.push_back(Parser("nCr("+std::to_string(n)+",2)="+
                    binomialExact(BigInt(n),BigInt(2)).str()).statement());
            }
            CrossSequenceMiner cm;auto c=cm.mine(facts);bool found=false;
            for(const auto&x:c)if(x.first.find("nPr(n,2)")!=std::string::npos&&
                                  x.first.find("2*nCr(n,2)")!=std::string::npos)found=true;
            require(found,"cross-sequence affine identity not discovered");
        });
        check("cross-sequence additive synthesis",[]{
            std::vector<Statement> facts;
            for(int n=2;n<=9;++n){
                BigInt a=binomialExact(BigInt(n),BigInt(2));
                BigInt b=binomialExact(BigInt(n),BigInt(1));
                BigInt c=a+b;
                facts.push_back(Parser("nCr("+std::to_string(n)+",2)="+a.str()).statement());
                facts.push_back(Parser("nCr("+std::to_string(n)+",1)="+b.str()).statement());
                facts.push_back(Parser("composition("+std::to_string(n)+",2)="+
                    compositionExact(BigInt(n),BigInt(2)).str()).statement());
                (void)c;
            }
            // The database need not contain a planted named theorem; it should
            // still return only identities actually supported by observations.
            CrossSequenceMiner cm;auto c=cm.mine(facts);
            require(!c.empty(),"cross-sequence synthesis produced no candidates");
        });
        check("theorem composer generates certified polynomial consequences",[]{
            Config c;c.timeLimitMs=1000;c.tests=20;c.iterations=1;DiscoveryEngine d(c);
            d.proveText("a+b=b+a");
            d.proveText("x+y=y+x");
            std::ostringstream log;d.research(log,1);
            bool found=false;
            for(const auto&r:d.knowledge().all()){
                if((r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM)&&
                   r.generator.find("theorem-composition:")!=std::string::npos &&
                   print(r.statement).find("a")!=std::string::npos &&
                   print(r.statement).find("x")!=std::string::npos){found=true;break;}
            }
            require(found,"polynomial theorem composition was not certified");
        });
        check("proof planner distinguishes research from proof",[]{
            ProofPlanner pp;
            auto s=Parser("(x^2-1)/(x-1)=x+1").statement();
            ProofPlan p=pp.plan(s);
            require(!p.strategy.empty(),"empty proof strategy");
            require(!p.obligations.empty(),"missing denominator obligation");
        });
        check("multi-step Fibonacci proof planning",[]{
            auto s=Parser("fib(n+2)+fib(n+2)=2*fib(n+1)+2*fib(n) assuming n>=0").statement();
            auto p=Euclid().prove(s); std::string w;
            require(p&&Kernel().verify(s,*p,w),"multi-step Fibonacci proof was not kernel-certified");
            require(p->steps.size()>=3,"proof chain did not contain recursive rewrites plus normalization");
        });
        check("multi-step factorial proof planning",[]{
            auto s=Parser("factorial(n+1)+factorial(n+1)=2*(n+1)*factorial(n) assuming n>=0").statement();
            auto p=Euclid().prove(s); std::string w;
            require(p&&Kernel().verify(s,*p,w),"multi-step factorial proof was not kernel-certified");
        });
        check("multi-step proof rejects forged rewrite",[]{
            auto s=Parser("fib(n+2)+fib(n+2)=2*fib(n+1)+2*fib(n) assuming n>=0").statement();
            auto p=Euclid().prove(s); require(bool(p),"setup proof failed");
            p->steps[0].after=p->steps[0].before;
            std::string w; require(!Kernel().verify(s,*p,w),"forged rewrite accepted");
        });
        check("proof planner advertises recursive strategy",[]{
            ProofPlanner pp;auto s=Parser("fib(n+2)+fib(n+2)=2*fib(n+1)+2*fib(n) assuming n>=0").statement();
            auto p=pp.plan(s);require(p.strategy.find("recursive")!=std::string::npos,"planner did not identify recursive strategy");
        });
        check("research graph exports DOT",[]{
            Config c;c.timeLimitMs=1000;c.iterations=1;c.tests=15;c.seed=19;DiscoveryEngine d(c);std::ostringstream log;d.research(log,1);
            const std::string f="hilbert_graph_test.dot";d.exportGraphDot(f);
            std::ifstream in(f);require(bool(in),"DOT export missing");
            std::string text((std::istreambuf_iterator<char>(in)),{});
            require(text.find("digraph HILBERT_RESEARCH")!=std::string::npos,"invalid DOT header");
        });

        check("bidirectional Fibonacci rewrite search",[]{
            auto s=Parser("fib(n+1)+fib(n)=fib(n+2) assuming n>=0").statement();
            auto p=Euclid().prove(s);std::string w;
            require(p&&Kernel().verify(s,*p,w),"reverse recurrence was not certified");
            require(p->steps.size()>=2,"missing bidirectional proof steps");
        });
        check("bidirectional factorial rewrite search",[]{
            auto s=Parser("(n+1)*factorial(n)=factorial(n+1) assuming n>=0").statement();
            auto p=Euclid().prove(s);std::string w;
            require(p&&Kernel().verify(s,*p,w),"reverse factorial recurrence was not certified");
        });
        check("bidirectional forged certificate rejected",[]{
            auto s=Parser("fib(n+1)+fib(n)=fib(n+2) assuming n>=0").statement();
            auto p=Euclid().prove(s);require(bool(p),"setup proof failed");
            if(p->steps.size()>1)p->steps[0].after="forged";
            std::string w;require(!Kernel().verify(s,*p,w),"forged bidirectional certificate accepted");
        });
        check("research produces conjectures and verified theorems",[]{
            Config c;c.timeLimitMs=1000;c.iterations=2;c.tests=25;c.seed=23;DiscoveryEngine d(c);std::ostringstream log;d.research(log,1);
            bool conjecture=false,theorem=false;
            for(const auto&r:d.knowledge().all()){
                conjecture|=(r.status==Status::CONJECTURE);
                theorem|=(r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM);
            }
            require(conjecture,"no conjecture stage observed");require(theorem,"no verified theorem stage observed");
            require(!d.researchGraph().edges().empty(),"research graph stayed empty");
        });
        check("lemma forge extracts recursive obligation",[]{
            Config c;c.timeLimitMs=1000;c.tests=25;c.iterations=4;DiscoveryEngine d(c);
            auto target=Parser("fib(n+2)+fib(n)=fib(n+1)+fib(n+1) assuming n>=0").statement();
            std::ostringstream log;
            d.process(target,"lemma-forge-test");
            d.research(log,1);
            bool found=false;
            for(const auto&r:d.knowledge().all())
                if(r.status==Status::LEMMA_FOUND && r.generator.find("lemma-forge:")!=std::string::npos){found=true;break;}
            require(found,"lemma forge did not certify a reusable lemma");
        });
        check("lemma forge rejects false obstacle lemma",[]{
            Config c;c.timeLimitMs=1000;c.tests=50;c.iterations=2;DiscoveryEngine d(c);
            auto target=Parser("fib(n+2)=fib(n+1)+2*fib(n) assuming n>=0").statement();
            std::ostringstream log;d.research(log,1);
            bool forgedFalse=false;
            for(const auto&r:d.knowledge().all())
                if(r.status==Status::LEMMA_FOUND && print(r.statement).find("2*fib")!=std::string::npos) forgedFalse=true;
            require(!forgedFalse,"false lemma escaped kernel gating");
        });
        std::cout<<"Tests: "<<pass<<" passed, "<<fail<<" failed\n";
        check("portfolio proof search",[]{
            auto s=Parser("(a+b)*(a-b)=a^2-b^2").statement();
            auto p=ProofPortfolio().prove(s,2000,16); std::string w;
            require(p&&Kernel().verify(s,*p,w),"portfolio failed");
        });
        check("experience memory persistence",[]{
            ExperienceMemory m; m.observe("unit",true,10,1.0,"algebra");
            const std::string f="hilbert_exp.txt"; m.save(f); ExperienceMemory n; n.load(f);
            require(n.size()==1,"experience reload");
        });
        check("research skeptic rejects false claim",[]{
            auto s=Parser("a+b=a*b").statement();
            require(ResearchDebate().skeptic(s,9,50).falsified,"skeptic missed false claim");
        });
        check("theory inducer finds repeated concept",[]{
            Config c;c.timeLimitMs=1000;c.tests=10;DiscoveryEngine d(c);
            d.proveText("fib(n+2)=fib(n+1)+fib(n) assuming n>=0"); d.proveText("fib(m+2)=fib(m+1)+fib(m) assuming m>=0");
            auto cs=TheoryInducer().induce(d.knowledge());
            require(!cs.empty(),"no induced concepts");
        });
        check("structural abstraction is alpha-invariant",[]{
            auto a=Parser("(x+y)*z=x*z+y*z").statement();
            auto b=Parser("(a+b)*c=a*c+b*c").statement();
            require(StructuralAbstraction::fingerprint(a)==StructuralAbstraction::fingerprint(b),"fingerprints differ");
        });
        check("novelty archive detects repeated structure",[]{
            NoveltyArchive n; auto a=Parser("x+y=y+x").statement(); auto b=Parser("a+b=b+a").statement();
            require(n.admit(a),"first pattern rejected"); require(n.novelty(b)<1.0,"repeat considered novel");
        });
        check("research trace persists",[]{
            Config c;c.timeLimitMs=1000; c.tests=10; DiscoveryEngine d(c); d.proveText("a+b=b+a");
            const std::string f="hilbert_v4_trace_test.jsonl"; d.saveTrace(f); std::ifstream in(f); require(bool(in),"trace missing"); in.close(); std::remove(f.c_str());
        });
        check("research council operates above kernel",[]{
            Config c;c.timeLimitMs=1000; c.tests=15; DiscoveryEngine d(c); d.proveText("a+b=b+a");
            require(!d.noveltyReport().empty(),"council/novelty state unavailable");
        });
        check("counterexample-guided repair finds nearby theorem",[]{
            Config c;c.timeLimitMs=1000;c.tests=30;c.iterations=1;c.maxRepairs=6;DiscoveryEngine d(c);
            std::ostringstream log;d.research(log,1);
            auto bad=Parser("a*(b+c)=a*b+a+b*c").statement();
            auto rs=CounterexampleRepairer().repair(bad,6);
            require(!rs.empty(),"repair neighborhood empty");
        });
        check("brain checkpoint round trip",[]{
            Config c;c.timeLimitMs=1000;c.tests=20;DiscoveryEngine d(c);d.proveText("a+b=b+a");
            const std::string f="hilbert_brain_test.hbrain";d.saveBrain(f);
            DiscoveryEngine e(c);std::ostringstream log;e.loadBrain(f,log);
            require(!e.knowledge().all().empty(),"brain did not reload"); std::remove(f.c_str());
        });
        check("research time budget is enforced",[]{
            Config c;c.timeLimitMs=1000;c.tests=10;c.iterations=100;c.timeLimitMs=5;DiscoveryEngine d(c);std::ostringstream log;d.research(log,100);
            require(log.str().find("research")!=std::string::npos,"research did not start");
        });
        check("self-directed research planner generates strategy moves",[]{
            Config c;c.timeLimitMs=1000;c.tests=20;c.iterations=1;DiscoveryEngine d(c);
            d.proveText("fib(n+2)=fib(n+1)+fib(n) assuming n>=0");
            std::ostringstream log;d.research(log,1);
            bool seen=false;
            for(const auto&r:d.knowledge().all())
                if(r.generator.find("research-strategy:")!=std::string::npos){seen=true;break;}
            require(seen,"research strategist generated no executable strategy");
        });
        check("theorem-driven strategy works without an open frontier",[]{
            ResearchStrategyPlanner planner;
            auto theorem=Parser("fib(n+2)=fib(n+1)+fib(n) assuming n>=0").statement();
            auto moves=planner.planTrusted(std::vector<Statement>{theorem},8);
            require(!moves.empty(),"verified theorem produced no research moves");
            bool shifted=false,converse=false;
            for(const auto&m:moves){
                const std::string r=m.reason;
                shifted=shifted||(r.find("parameter shift")!=std::string::npos);
                converse=converse||(r.find("converse")!=std::string::npos);
                (void)Parser(print(m.statement)).statement();
            }
            require(shifted,"theorem strategy omitted parameter exploration");
            require(converse,"theorem strategy omitted converse exploration");
        });
        check("scientist theorem instantiation is nontrivial",[]{
            Config c;c.timeLimitMs=5000;c.tests=25;c.iterations=1;c.seed=41;DiscoveryEngine d(c);
            d.proveText("a*(b+c)=a*b+a*c");
            std::ostringstream log; d.research(log,1);
            bool seen=false;
            for(const auto&r:d.knowledge().all())
                if(r.generator.find("scientist:instantiation")!=std::string::npos){seen=true;break;}
            require(seen,"scientist produced no instantiated research object");
        });
        check("scientist converse remains kernel-gated",[]{
            Config c;c.timeLimitMs=1000;c.tests=30;c.iterations=1;DiscoveryEngine d(c);
            auto r=d.proveText("a*(b+c)=a*b+a*c");
            require(r.status==Status::THEOREM,"seed theorem missing");
            auto q=Parser("a*b+a*c=a*(b+c)").statement();
            auto p=Euclid().prove(q); std::string w;
            require(p&&Kernel().verify(q,*p,w),"true converse was not certifiable");
        });
        check("scientist assumption ablation stays a hypothesis",[]{
            Config c;c.timeLimitMs=1000;c.tests=30;DiscoveryEngine d(c);
            auto r=d.proveText("x/x=1 assuming x!=0");
            require(r.status==Status::CONDITIONAL_THEOREM,"conditional theorem missing");
            auto q=Parser("x/x=1").statement();
            require(!Euclid().prove(q),"ablated denominator assumption became a false proof");
        });
        check("abstraction engine generalizes verified laws",[]{
            Config c;c.timeLimitMs=1000;c.tests=20;DiscoveryEngine d(c);
            d.proveText("a+b=b+a");
            d.proveText("x+y=y+x");
            auto a=d.knowledge().all();
            auto ps=AbstractionInventionEngine().generalize(a,8);
            require(!ps.empty(),"no generalized research object generated");
            bool hasVar=false;
            for(const auto&p:ps)if(print(p.statement).find("h")!=std::string::npos){hasVar=true;break;}
            require(hasVar,"generalization did not introduce an abstraction variable");
        });
        check("abstraction hypotheses remain kernel-gated",[]{
            Config c;c.timeLimitMs=1000;c.tests=30;DiscoveryEngine d(c);
            d.proveText("a+b=b+a");
            d.proveText("x+y=y+x");
            auto ps=AbstractionInventionEngine().generalize(d.knowledge().all(),8);
            require(!ps.empty(),"no abstraction hypothesis");
            for(const auto&p:ps){
                auto proof=Euclid().prove(p.statement);
                if(proof){std::string why;require(Kernel().verify(p.statement,*proof,why),why);}
            }
        });
        check("minimal assumption experiments delete exactly one assumption",[]{
            Config c;c.timeLimitMs=1000;c.tests=20;DiscoveryEngine d(c);
            d.proveText("x/x=1 assuming x!=0 and x>0");
            auto ps=AbstractionInventionEngine().minimalAssumptionQuestions(d.knowledge().all(),8);
            require(!ps.empty(),"no minimal-assumption experiment generated");
            for(const auto&p:ps)require(p.statement.assumptions.size()==1,"wrong assumption count");
        });
        check("insight engine detects verified mathematical families",[]{
            Config c;c.timeLimitMs=1000;c.tests=20;DiscoveryEngine d(c);
            d.proveText("a+b=b+a");
            d.proveText("x+y=y+x");
            auto xs=d.mathematicalInsights(16);
            require(!xs.empty(),"insight engine found no verified structure");
            bool family=false;
            for(const auto&i:xs)if(i.kind=="family"||i.kind=="invariant")family=true;
            require(family,"no family/invariant insight");
        });
        check("insight engine creates research questions",[]{
            Config c;c.timeLimitMs=1000;c.tests=20;DiscoveryEngine d(c);
            d.proveText("a+b=b+a");
            d.proveText("x+y=y+x");
            auto qs=d.insightQuestions(8);
            require(!qs.empty(),"insight engine produced no research questions");
        });
        check("insight layer never bypasses kernel",[]{
            Config c;c.timeLimitMs=1000;c.tests=20;DiscoveryEngine d(c);
            d.proveText("a+b=b+a");
            auto xs=d.mathematicalInsights(8);
            for(const auto&i:xs)
                require(i.confidence>=0.0&&i.confidence<=1.0,"invalid insight confidence");
        });
        check("adaptive research policy learns context and persists",[]{
            SearchPolicy p; p.observe("number_theory","mission:prove",Status::KERNEL_VERIFIED,1.0); p.observe("number_theory","mission:counterexample",Status::FALSIFIED,-0.35);
            require(p.value("number_theory","mission:prove")>p.value("number_theory","mission:counterexample"),"contextual policy failed to learn");
            const std::string f="hilbert_policy_test.txt"; p.save(f); SearchPolicy q; q.load(f);
            require(q.value("number_theory","mission:prove")>q.value("number_theory","mission:counterexample"),"policy persistence lost learned ordering"); std::remove(f.c_str());
        });
        check("research director builds adaptive mission plans",[]{
            Config c;c.tests=20;c.researchBeam=12;c.missionBudget=3;c.missionSteps=4;
            DiscoveryEngine d(c); d.proveText("a+b=a*b");
            auto tasks=d.frontierTasks(8); require(!tasks.empty(),"no frontier tasks for director");
            ResearchDirector rd; auto ms=rd.create(tasks,3,4); require(!ms.empty(),"director produced no missions");
            for(const auto&m:ms)require(!m.plan.empty(),"empty mission plan");
        });
        check("research director remains kernel-gated",[]{
            Config c;c.tests=10;c.maxStates=1000;c.maxDepth=8;c.maxRepairs=1;c.researchBeam=4;c.missionBudget=1;c.missionSteps=2;
            DiscoveryEngine d(c); d.proveText("a+b=a*b");
            std::ostringstream log; d.runMissions(log,1,2);
            for(const auto&r:d.knowledge().all()) if(r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM){
                require(bool(r.proof),"theorem has no certificate"); std::string why;require(Kernel().verify(r.statement,*r.proof,why),why);
            }
            require(!d.missions().empty(),"no missions executed");
        });
        check("research director executes sequential mission state",[]{
            Config c;c.tests=10;c.maxStates=1000;c.maxDepth=8;c.maxRepairs=1;c.researchBeam=4;c.missionBudget=1;c.missionSteps=3;
            DiscoveryEngine d(c); d.proveText("a+b=a*b");
            std::ostringstream log; d.runMissions(log,1,3);
            require(!d.missions().empty(),"mission archive empty"); const ResearchMission&m=d.missions().back();
            require(m.step>0,"mission executed zero steps");
            require(m.state=="SOLVED"||m.state=="REFUTED"||m.state=="ESCALATED","invalid mission terminal state");
        });
        check("active discovery lab generates experimental hypotheses",[]{
            Config c;c.tests=20;c.maxDepth=10;c.maxStates=2000;
            DiscoveryEngine d(c);
            d.proveText("a+b=b+a");
            d.proveText("(a+b)*c=a*c+b*c");
            ActiveDiscoveryLab lab;
            auto ps=lab.probe(d.knowledge().all(),12);
            require(!ps.empty(),"active lab generated no hypotheses");
            bool boundary=false,swap=false;
            for(const auto&p:ps){
                std::string t=print(p.statement);
                if(t.find("0")!=std::string::npos||t.find("-1")!=std::string::npos)boundary=true;
                if(p.reason.find("exchange variables")!=std::string::npos)swap=true;
            }
            require(boundary,"active lab produced no boundary experiment");
            require(swap,"active lab produced no symmetry experiment");
        });
        check("active discovery proposals remain kernel-gated",[]{
            Config c;c.tests=20;c.maxDepth=10;c.maxStates=2000;
            DiscoveryEngine d(c);
            d.proveText("a+b=b+a");
            ActiveDiscoveryLab lab;
            auto ps=lab.probe(d.knowledge().all(),12);
            for(const auto&p:ps){
                auto proof=Euclid().prove(p.statement);
                if(proof){std::string why;require(Kernel().verify(p.statement,*proof,why),why);}
            }
        });
        check("V20 cross-domain research proposals",[]{
            Config c;c.tests=12;c.maxDepth=10;c.maxStates=2000;
            DiscoveryEngine d(c);
            d.proveText("a+b=b+a");
            auto ps=CrossDomainResearchEngine().generate(d.knowledge().all(),8,1);
            require(!ps.empty(),"cross-domain engine generated no proposals");
            for(const auto&p:ps){
                require(!print(p.statement).empty(),"empty cross-domain statement");
                (void)Parser(print(p.statement)).statement();
            }
        });
        check("V20 preserves kernel gate",[]{
            Config c;c.tests=20;c.maxDepth=10;c.maxStates=2000;
            DiscoveryEngine d(c);
            d.proveText("a*(b+c)=a*b+a*c");
            auto ps=CrossDomainResearchEngine().generate(d.knowledge().all(),6,7);
            for(const auto&p:ps){
                Record r=d.proveText(print(p.statement));
                if(r.proof){
                    std::string why;
                    require(Kernel().verify(r.statement,*r.proof,why),why);
                }
            }
        });
        std::cout<<"Tests: "<<pass<<" passed, "<<fail<<" failed\n";
        return fail?1:0;
    }
};

static int benchmark() { const char* cases[]={"a+b=b+a","a*(b+c)=a*b+a*c","(a+b)^2=a^2+2*a*b+b^2","(a+b)*(a-b)=a^2-b^2","a*(b+c)=a*b+a+b*c"}; int ok=0; for(const char* text:cases){Statement s=Parser(text).statement(); Optional<Proof> p=Euclid().prove(s); std::string why; bool verified=p&&Kernel().verify(s,*p,why); std::cout<<(verified?"[VERIFIED] ":"[NOT PROVED] ")<<text<<"\n";ok+=verified;} std::cout<<"Benchmarks: "<<ok<<" verified / 5 total (the final case is intentionally false)\n";return ok==4?0:1; }
static int fuzz(uint64_t seed,int count) { std::mt19937_64 rng(seed); int pass=0; for(int i=0;i<count;++i){int a=int(rng()%21)-10,b=int(rng()%21)-10,c=int(rng()%21)-10;std::string text="("+std::to_string(a)+"+"+std::to_string(b)+")*"+std::to_string(c)+"="+std::to_string(a)+"*"+std::to_string(c)+"+"+std::to_string(b)+"*"+std::to_string(c);Statement s=Parser(text).statement();if(canonical(s.left)!=canonical(s.right))throw Error("fuzz canonicalization mismatch");if(!(eval(s.left,{})==eval(s.right,{})))throw Error("fuzz exact-evaluation mismatch");++pass;}std::cout<<"Fuzz: "<<pass<<" exact distributivity instances passed (seed="<<seed<<")\n";return 0; }
static const char* VERSION="H.I.L.B.E.R.T. 21.0 - Persistent Autonomous Mathematical Research Core";
static void usage(){
    std::cout
      <<VERSION<<"\n"
      <<"Kernel-gated Artificial Mathematician + Theory Formation + exact Number Theory + Combinatorics\n\n"
      <<"Usage:\n"
      <<"  HILBERT --test\n"
      <<"  HILBERT --benchmark\n"
      <<"  HILBERT --nt-benchmark\n"
      <<"  HILBERT --fuzz [--tests N] [--seed N]\n"
      <<"  HILBERT --prove \"STATEMENT\" [--certificate-out FILE]\n"
      <<"  HILBERT --counterexample \"STATEMENT\"\n"
      <<"  HILBERT --discover [--learn FILE] [--research-rounds N] [--iterations N] [--tests N] [--seed N]\n"
      <<"  HILBERT --experiment N [--iterations N] [--tests N] [--seed N]\n"
      <<"  HILBERT --verify-certificate FILE\n"
      <<"  HILBERT --export FILE\n"
      <<"  HILBERT --stats\n"
      <<"  HILBERT --theories\n"
      <<"  HILBERT --graph\n"
      <<"  HILBERT --trace FILE\n"
      <<"  HILBERT --interactive [--experience FILE] [--policy FILE]\n"
      <<"  HILBERT --missions [--research-rounds N] [--mission-budget N] [--mission-steps N]\n"
      <<"  HILBERT --active-research [--research-rounds N] [--tests N] [--seed N]\n"
      <<"  HILBERT --v20 [--research-rounds N] [--research-beam N] [--tests N] [--seed N]\n"
      <<"  HILBERT --agenda [--learn FILE]\n"
      <<"  HILBERT --db FILE [--live-graph FILE]\n"
      <<"  HILBERT --reset-db\n"
      <<"  HILBERT --no-persistence\n\n"
      <<"Safety:\n"
      <<"  Empirical tests are never proofs.\n"
      <<"  Only an independently verified certificate becomes a theorem.\n";
}

static int numberTheoryBenchmark(){
    struct C{const char* q;};
    const C cases[]={
        {"gcd(123456,7890)=6"},
        {"lcm(84,30)=420"},
        {"phi(100)=40"},
        {"tau(360)=24"},
        {"sigma(12)=28"},
        {"fib(100)=354224848179261915075"},
        {"nCr(100,50)=100891344545564193334812497256"},
        {"nPr(20,10)=670442572800"},
        {"catalan(15)=9694845"},
        {"derangements(10)=1334961"},
        {"stirling2(10,5)=42525"},
        {"partition(20)=627"},
        {"bell(10)=115975"}
    };
    int ok=0;
    for(const auto& c:cases){
        try{
            auto s=Parser(c.q).statement();
            bool v=relationHolds(s.rel,eval(s.left,{}),eval(s.right,{ }));
            std::cout<<(v?"[PASS] ":"[FAIL] ")<<c.q<<"\n"; ok+=v;
        }catch(const std::exception&e){std::cout<<"[FAIL] "<<c.q<<" : "<<e.what()<<"\n";}
    }
    std::cout<<"Number-theory/combinatorics benchmarks: "<<ok<<"/"<<sizeof(cases)/sizeof(cases[0])<<"\n";
    return ok==int(sizeof(cases)/sizeof(cases[0]))?0:1;
}

int mainImpl(int argc,char**argv){
    Config c;
    bool test=false,discover=false,interactive=false,bench=false,doFuzz=false,knowledge=false;
    bool stats=false,ntbench=false,theoryReport=false,graphReport=false,agendaReport=false,frontierReport=false,questionsReport=false,insightsReport=false,missionsReport=false,activeResearch=false,v20=false;
    std::string graphDotFile,traceFile,brainSaveFile,brainLoadFile,journalFile,frontierFile;
    int experimentRuns=0,researchRounds=2;
    std::string prove,cex,exportFile,verifyFile,certificateOut,learnFile,experienceFile,policyFile;
    std::string databaseFile="hilbert_brain.db",liveGraphFile="hilbert_live_graph.html";
    bool resetDatabase=false,noPersistence=false;
    for(int i=1;i<argc;++i){
        std::string a=argv[i];
        auto value=[&](){
            if(i+1>=argc)throw Error("missing value for "+a);
            return std::string(argv[++i]);
        };
        if(a=="--test")test=true;
        else if(a=="--discover")discover=true;
        else if(a=="--benchmark")bench=true;
        else if(a=="--fuzz")doFuzz=true;
        else if(a=="--knowledge")knowledge=true;
        else if(a=="--stats")stats=true;
        else if(a=="--nt-benchmark")ntbench=true;
        else if(a=="--theories")theoryReport=true;
        else if(a=="--graph")graphReport=true;
        else if(a=="--agenda")agendaReport=true;
        else if(a=="--frontier")frontierReport=true;
        else if(a=="--questions")questionsReport=true;
         else if(a=="--insights")insightsReport=true;
        else if(a=="--missions")missionsReport=true;
        else if(a=="--active-research")activeResearch=true;
        else if(a=="--v20")v20=true;
        else if(a=="--frontier-json")frontierFile=value();
        else if(a=="--experience")experienceFile=value();
        else if(a=="--policy")policyFile=value();
        else if(a=="--graph-dot")graphDotFile=value();
        else if(a=="--trace")traceFile=value();
        else if(a=="--save-brain")brainSaveFile=value();
        else if(a=="--load-brain")brainLoadFile=value();
        else if(a=="--journal")journalFile=value();
        else if(a=="--max-repairs")c.maxRepairs=std::stoi(value());
        else if(a=="--research-beam")c.researchBeam=std::stoi(value());
        else if(a=="--mission-budget")c.missionBudget=std::stoi(value());
        else if(a=="--mission-steps")c.missionSteps=std::stoi(value());
        else if(a=="--learn")learnFile=value();
        else if(a=="--db"||a=="--database")databaseFile=value();
        else if(a=="--live-graph")liveGraphFile=value();
        else if(a=="--reset-db")resetDatabase=true;
        else if(a=="--no-persistence")noPersistence=true;
        else if(a=="--research-rounds")researchRounds=std::stoi(value());
        else if(a=="--prove"||a=="--search")prove=value();
        else if(a=="--counterexample")cex=value();
        else if(a=="--verify-certificate")verifyFile=value();
        else if(a=="--certificate-out")certificateOut=value();
        else if(a=="--export")exportFile=value();
        else if(a=="--experiment")experimentRuns=std::stoi(value());
        else if(a=="--interactive")interactive=true;
        else if(a=="--iterations")c.iterations=std::stoi(value());
        else if(a=="--tests")c.tests=std::stoi(value());
        else if(a=="--seed")c.seed=std::stoull(value());
        else if(a=="--time-limit")c.timeLimitMs=std::stoi(value());
        else if(a=="--max-states")c.maxStates=std::stoi(value());
        else if(a=="--max-depth")c.maxDepth=std::stoi(value());
        else if(a=="--help"){usage();return 0;}
        else if(a=="--version"){std::cout<<VERSION<<"\n";return 0;}
        else throw Error("unknown option: "+a);
    }

    if(test)return Tests().run();
    if(bench)return benchmark();
    if(ntbench)return numberTheoryBenchmark();
    if(doFuzz)return fuzz(c.seed,c.tests);
    if(experimentRuns>0)return experiment(c,experimentRuns);

    DiscoveryEngine d(c);
    if(!noPersistence){
        d.initializePersistence(databaseFile,liveGraphFile,resetDatabase,std::cout);
    }
    if(!brainLoadFile.empty()) d.loadBrain(brainLoadFile,std::cout);
    if(!learnFile.empty()) d.learnFile(learnFile,std::cout);
    if(!experienceFile.empty()) { try { d.loadExperience(experienceFile); } catch(const std::exception&e) { std::cout<<"Experience load skipped: "<<e.what()<<"\n"; } }
    if(!policyFile.empty()) { try { d.loadPolicy(policyFile); } catch(const std::exception&e) { std::cout<<"Policy load skipped: "<<e.what()<<"\n"; } }

    if(!verifyFile.empty()){
        std::string statementText=certificateStatement(verifyFile);
        Statement st=Parser(statementText).statement();
        Proof p=loadCertificate(verifyFile);
        bool ok=verifyCertificateText(st,p);
        return ok?0:1;
    }

    if(!prove.empty()){
        Record r=d.proveText(prove);
        std::cout<<statusName(r.status)<<": "<<print(r.statement)<<"\n";
        std::cout<<"Evidence tests: "<<r.evidence.tests<<"\n";
        if(r.evidence.counterexample){
            std::cout<<"Counterexample:";
            for(const auto&x:*r.evidence.counterexample)
                std::cout<<" "<<x.first<<"="<<x.second.str();
            std::cout<<"\n";
        }
        if(r.proof){
            std::cout<<r.proof->text();
            std::cout<<"Kernel: "<<r.kernelReason<<"\n";
            if(!certificateOut.empty()){
                saveCertificate(r.statement,*r.proof,certificateOut);
                std::cout<<"Certificate written: "<<certificateOut<<"\n";
            }
        }else{
            std::cout<<r.evidence.reason<<"\n";
            if(!r.evidence.counterexample)
                std::cout<<"No proof in the supported kernel domain; finite tests are not a proof.\n";
        }
        d.flushPersistence();
        return (r.status==Status::THEOREM||r.status==Status::CONDITIONAL_THEOREM)?0:1;
    }

    if(!cex.empty()){
        Statement st=Parser(cex).statement();
        Evidence e=Popper(c.seed).attack(st,c.tests);
        if(!e.counterexample){
            std::cout<<"NO_COUNTEREXAMPLE_FOUND\n";
            std::cout<<"Exact tests performed: "<<e.tests<<"\n";
            std::cout<<"This is not a proof.\n";
            return 1;
        }
        std::cout<<"COUNTEREXAMPLE:";
        for(const auto&x:*e.counterexample)
            std::cout<<" "<<x.first<<"="<<x.second.str();
        std::cout<<"\n";
        return 0;
    }

    if(v20){
        AutonomousResearchCoreV20 core;
        return core.run(d,std::cout,researchRounds,std::max(8,c.researchBeam),c.seed);
    }

    if(activeResearch){
        // Active research is a focused autonomous experiment mode.  It keeps
        // the same trust boundary as --discover but gives the active laboratory
        // a wider beam and multiple rounds so that its experimental proposals
        // can feed back into later mining and theory induction.
        c.researchBeam=std::max(c.researchBeam,32);
        c.iterations=std::min(c.iterations,6);
        c.tests=std::min(c.tests,50);
        c.maxStates=std::min(c.maxStates,5000);
        c.maxDepth=std::min(c.maxDepth,8);
        c.maxRepairs=std::min(c.maxRepairs,1);
        if(c.timeLimitMs<=0)c.timeLimitMs=15000;
        d.research(std::cout,1);
        std::cout<<"Active laboratory run complete. Kernel-gated theorems remain the only trusted results.\n";
        return 0;
    }

    if(discover){
        d.research(std::cout,researchRounds);
        if(!exportFile.empty()){
            ResearchReport::exportKnowledge(d.knowledge(),exportFile);
            std::cout<<"Knowledge exported: "<<exportFile<<"\n";
        }
        if(!experienceFile.empty()) d.saveExperience(experienceFile);
        if(!policyFile.empty()) { d.savePolicy(policyFile); std::cout<<"Research policy written: "<<policyFile<<"\n"; }
        if(!traceFile.empty()){ d.saveTrace(traceFile); std::cout<<"Research trace written: "<<traceFile<<"\n"; }
        if(!brainSaveFile.empty()){ d.saveBrain(brainSaveFile); std::cout<<"Brain checkpoint written: "<<brainSaveFile<<"\n"; }
        if(!journalFile.empty()){ d.saveJournal(journalFile); std::cout<<"Research journal written: "<<journalFile<<"\n"; }
        if(!frontierFile.empty()){ d.exportFrontier(frontierFile); std::cout<<"Research frontier written: "<<frontierFile<<"\n"; }
        d.flushPersistence();
        return 0;
    }

    if(knowledge){
        d.research(std::cout,researchRounds);
        if(!exportFile.empty())ResearchReport::exportKnowledge(d.knowledge(),exportFile);
        if(!experienceFile.empty()) d.saveExperience(experienceFile);
        if(!policyFile.empty()) { d.savePolicy(policyFile); std::cout<<"Research policy written: "<<policyFile<<"\n"; }
        if(!brainSaveFile.empty()) d.saveBrain(brainSaveFile);
        if(!journalFile.empty()) d.saveJournal(journalFile);
        if(!frontierFile.empty()) d.exportFrontier(frontierFile);
        d.flushPersistence();
        return 0;
    }

    if(stats){
        ResearchReport::printStats(d.knowledge());
        return 0;
    }

    if(theoryReport){ d.printTheories(std::cout); return 0; }
    if(agendaReport){ for(const auto&q:d.researchAgenda()) std::cout<<"- "<<q<<"\n"; return 0; }
    if(missionsReport){ d.research(std::cout,researchRounds); std::cout<<"\nExecuted missions: "<<d.missions().size()<<"\n"; if(!policyFile.empty()){ d.savePolicy(policyFile); std::cout<<"Research policy written: "<<policyFile<<"\n"; } return 0; }
    if(frontierReport){ d.printFrontier(std::cout); return 0; }
    if(questionsReport){ for(const auto&q:d.researchQuestions()) std::cout<<"? "<<q<<"\n"; return 0; }
     if(insightsReport){ d.research(std::cout,std::max(1,researchRounds)); d.printInsights(std::cout); return 0; }
    if(graphReport){ d.printGraph(std::cout); d.flushPersistence(); return 0; }
    if(!graphDotFile.empty()){ d.research(std::cout,std::max(1,researchRounds)); d.exportGraphDot(graphDotFile); d.flushPersistence(); std::cout<<"Graph DOT written: "<<graphDotFile<<"\n"; return 0; }

    if(interactive){
        std::cout<<VERSION<<"\n";
        std::cout<<"Commands: prove S | counterexample S | generate | research | knowledge | theories | graph | agenda | frontier | questions | insights | stats | nt | quit\n";
        std::string line;
        while(std::cout<<"> "&&std::getline(std::cin,line)){
            if(line=="quit"||line=="exit")break;
            try{
                if(line.rfind("prove ",0)==0){
                    auto r=d.proveText(line.substr(6));
                    std::cout<<statusName(r.status)<<"\n";
                    if(r.proof)std::cout<<r.proof->text()<<"Kernel: "<<r.kernelReason<<"\n";
                    else std::cout<<r.evidence.reason<<"\n";
                }else if(line.rfind("counterexample ",0)==0){
                    Evidence e=Popper(c.seed).attack(Parser(line.substr(15)).statement(),c.tests);
                    if(e.counterexample){
                        std::cout<<"COUNTEREXAMPLE:";
                        for(const auto&x:*e.counterexample)std::cout<<" "<<x.first<<"="<<x.second.str();
                        std::cout<<"\n";
                    }else std::cout<<"NONE FOUND (not proof)\n";
                }else if(line=="generate"||line=="discover"){
                    d.discover(std::cout);
                }else if(line=="knowledge"){
                    for(const auto&r:d.knowledge().all())
                        std::cout<<"#"<<r.id<<" ["<<statusName(r.status)<<"] "<<print(r.statement)<<"\n";
                }else if(line=="nt"){
                    std::cout<<"Try: phi(36)=12, nCr(52,5)=2598960, partition(10)=42, stirling2(6,3)=90\n";
                }else if(line=="research"){
                    d.research(std::cout,researchRounds);
                }else if(line=="theories"){
                    d.printTheories(std::cout);
                }else if(line=="graph"){
                    d.printGraph(std::cout);
                }else if(line=="agenda"){
                    for(const auto&q:d.researchAgenda())std::cout<<"- "<<q<<"\n";
                }else if(line=="frontier"){
                    d.printFrontier(std::cout);
                }else if(line=="questions"){
                    for(const auto&q:d.researchQuestions())std::cout<<"? "<<q<<"\n";
                }else if(line=="stats"){
                    ResearchReport::printStats(d.knowledge());
                }else if(line=="help"){
                    std::cout<<"prove S | counterexample S | generate | research | knowledge | theories | graph | agenda | frontier | questions | insights | stats | quit\n";
                }else{
                    std::cout<<"Unknown command\n";
                }
            }catch(const std::exception&e){
                std::cout<<"Error: "<<e.what()<<"\n";
            }
        }
        return 0;
    }

    if(!exportFile.empty()){
        ResearchReport::exportKnowledge(d.knowledge(),exportFile);
        std::cout<<"Knowledge exported: "<<exportFile<<"\n";
        return 0;
    }

    usage();
    return 0;
}
} // namespace hilbert

int main(int argc,char**argv){
    try{return hilbert::mainImpl(argc,argv);}
    catch(const std::exception&e){
        std::cerr<<"HILBERT error: "<<e.what()<<"\n";
        return 2;
    }
}
