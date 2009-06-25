
#include "principalaxis.h"

static double fx;
static double ldt;
static double dmin;
static int nl;
static int nf;
static ap::real_2d_array v;
static ap::real_1d_array q0;
static ap::real_1d_array q1;
static double qa;
static double qb;
static double qc;
static double qd0;
static double qd1;
static double qf1;

static void svdfit(const int& n,
     const double& tol,
     ap::real_2d_array& ab,
     ap::real_1d_array& q);
static void directionalminimize(const int& n,
     const int& j,
     const int& nits,
     double& d2,
     double& x1,
     double& f1,
     const bool& fk,
     ap::real_1d_array& x,
     double& t,
     const double& machep,
     const double& h);
static double flin(const int& n,
     const int& j,
     const double& l,
     const ap::real_1d_array& x);
static void svdsort(const int& n,
     ap::real_1d_array& vecd,
     ap::real_2d_array& matv);
static void quadsearch(const int& n,
     ap::real_1d_array& x,
     double& t,
     const double& machep,
     const double& h);

/*************************************************************************
Поиск минимума функции N аргументов (N>=2) методом главных направлений.

Исходный код получен путем перевода оригинального кода из языка Fortran.
Автор оригинального кода и алгоритма в целом: Р.П.Брент.

Входные параметры:
    N   -   Размерность задачи. Не меньше 2.
    X   -   массив с нумерацией элементов от 1 до N. Начальная точка.
    T0  -   Требуемая точность. Алгоритм пытается найти такую точку, которая
            отличается от локального минимума не более, чем на
            T0+SquareRoot(MachineEpsilon)*NORM(Minimum).
    H0  -   Максимально допустимая длина шага. Должно по порядку
            соответствовать максимально возможному расстоянию от начальной
            точки до минимума. Если задано слишком большое или слишком маленькое
            значение, начальная скорость сходимости может быть слишком низкой.

Выходные параметры:
    X   -   найденное приближение к минимуму

Результат:
    Значение функции в найденной точке
*************************************************************************/
double principalaxisminimize(const int& n,
     ap::real_1d_array& x,
     const double& t0,
     const double& h0)
{
    double result;
    bool illc;
    int kl;
    int kt;
    int ktm;
    int i;
    int j;
    int k;
    int k2;
    int km1;
    int klmk;
    int ii;
    int im1;
    double s;
    double sl;
    double dn;
    double f1;
    double lds;
    double t;
    double h;
    double sf;
    double df;
    double m2;
    double m4;
    double small;
    double vsmall;
    double large;
    double vlarge;
    double scbd;
    double ldfac;
    double t2;
    double dni;
    double value;
    ap::real_1d_array d;
    ap::real_1d_array y;
    ap::real_1d_array z;
    double temparrayelement;

    d.setbounds(1, n);
    y.setbounds(1, n);
    z.setbounds(1, n);
    q0.setbounds(1, n);
    q1.setbounds(1, n);
    v.setbounds(1, n, 1, n);
    small = ap::sqr(ap::machineepsilon);
    vsmall = small*small;
    large = 1/small;
    vlarge = 1/vsmall;
    m2 = sqrt(ap::machineepsilon);
    m4 = sqrt(m2);
    scbd = 1;
    illc = false;
    ktm = 1;
    ldfac = 0.01;
    if( illc )
    {
        ldfac = 0.1;
    }
    kt = 0;
    nl = 0;
    nf = 1;
    fx = f(x);
    qf1 = fx;
    t = small+fabs(t0);
    t2 = t;
    dmin = small;
    h = h0;
    if( h<100*t )
    {
        h = 100*t;
    }
    ldt = h;
    for(i = 1; i <= n; i++)
    {
        for(j = 1; j <= n; j++)
        {
            v(i,j) = 0;
        }
        v(i,i) = 1;
    }
    d(1) = 0;
    qd0 = 0;
    for(i = 1; i <= n; i++)
    {
        q0(i) = x(i);
        q1(i) = x(i);
    }
    while(true)
    {
        sf = d(1);
        d(1) = 0;
        s = 0;
        value = fx;
        temparrayelement = d(1);
        directionalminimize(n, 1, 2, temparrayelement, s, value, false, x, t, ap::machineepsilon, h);
        d(1) = temparrayelement;
        if( s<=0 )
        {
            for(i = 1; i <= n; i++)
            {
                v(i,1) = -v(i,1);
            }
        }
        if( sf<=0.9*d(1)||0.9*sf>=d(1) )
        {
            for(i = 2; i <= n; i++)
            {
                d(i) = 0;
            }
        }
        for(k = 2; k <= n; k++)
        {
            for(i = 1; i <= n; i++)
            {
                y(i) = x(i);
            }
            sf = fx;
            if( kt>0 )
            {
                illc = true;
            }
            while(true)
            {
                kl = k;
                df = 0;
                if( illc )
                {
                    for(i = 1; i <= n; i++)
                    {
                        s = (0.1*ldt+t2*pow(double(10), double(kt)))*(ap::randomreal()-0.5);
                        z(i) = s;
                        for(j = 1; j <= n; j++)
                        {
                            x(j) = x(j)+s*v(j,i);
                        }
                    }
                    fx = f(x);
                    nf = nf+1;
                }
                for(k2 = k; k2 <= n; k2++)
                {
                    sl = fx;
                    s = 0;
                    value = fx;
                    temparrayelement = d(k2);
                    directionalminimize(n, k2, 2, temparrayelement, s, value, false, x, t, ap::machineepsilon, h);
                    d(k2) = temparrayelement;
                    if( illc )
                    {
                        s = d(k2)*ap::sqr(s+z(k2));
                    }
                    else
                    {
                        s = sl-fx;
                    }
                    if( df>s )
                    {
                        continue;
                    }
                    df = s;
                    kl = k2;
                }
                if( illc||df>=fabs(100*ap::machineepsilon*fx) )
                {
                    break;
                }
                illc = true;
            }
            km1 = k-1;
            for(k2 = 1; k2 <= km1; k2++)
            {
                s = 0;
                value = fx;
                temparrayelement = d(k2);
                directionalminimize(n, k2, 2, temparrayelement, s, value, false, x, t, ap::machineepsilon, h);
                d(k2) = temparrayelement;
            }
            f1 = fx;
            fx = sf;
            lds = 0;
            for(i = 1; i <= n; i++)
            {
                sl = x(i);
                x(i) = y(i);
                sl = sl-y(i);
                y(i) = sl;
                lds = lds+sl*sl;
            }
            lds = sqrt(lds);
            if( lds>small )
            {
                klmk = kl-k;
                if( klmk>=1 )
                {
                    for(ii = 1; ii <= klmk; ii++)
                    {
                        i = kl-ii;
                        for(j = 1; j <= n; j++)
                        {
                            v(j,i+1) = v(j,i);
                        }
                        d(i+1) = d(i);
                    }
                }
                d(k) = 0;
                for(i = 1; i <= n; i++)
                {
                    v(i,k) = y(i)/lds;
                }
                value = f1;
                temparrayelement = d(k);
                directionalminimize(n, k, 4, temparrayelement, lds, value, true, x, t, ap::machineepsilon, h);
                d(k) = temparrayelement;
                if( lds<=0 )
                {
                    lds = -lds;
                    for(i = 1; i <= n; i++)
                    {
                        v(i,k) = -v(i,k);
                    }
                }
            }
            ldt = ldfac*ldt;
            if( ldt<lds )
            {
                ldt = lds;
            }
            t2 = 0;
            for(i = 1; i <= n; i++)
            {
                t2 = t2+ap::sqr(x(i));
            }
            t2 = m2*sqrt(t2)+t;
            if( ldt>0.5*t2 )
            {
                kt = -1;
            }
            kt = kt+1;
            if( kt>ktm )
            {
                result = fx;
                return result;
            }
        }
        quadsearch(n, x, t, ap::machineepsilon, h);
        dn = 0;
        for(i = 1; i <= n; i++)
        {
            d(i) = 1/sqrt(d(i));
            if( dn<d(i) )
            {
                dn = d(i);
            }
        }
        for(j = 1; j <= n; j++)
        {
            s = d(j)/dn;
            for(i = 1; i <= n; i++)
            {
                v(i,j) = s*v(i,j);
            }
        }
        if( scbd>1 )
        {
            s = vlarge;
            for(i = 1; i <= n; i++)
            {
                sl = 0;
                for(j = 1; j <= n; j++)
                {
                    sl = sl+v(i,j)*v(i,j);
                }
                z(i) = sqrt(sl);
                if( z(i)<m4 )
                {
                    z(i) = m4;
                }
                if( s>z(i) )
                {
                    s = z(i);
                }
            }
            for(i = 1; i <= n; i++)
            {
                sl = s/z(i);
                z(i) = 1/sl;
                if( z(i)>scbd )
                {
                    sl = 1/scbd;
                    z(i) = scbd;
                }
                for(j = 1; j <= n; j++)
                {
                    v(i,j) = sl*v(i,j);
                }
            }
        }
        for(i = 2; i <= n; i++)
        {
            im1 = i-1;
            for(j = 1; j <= im1; j++)
            {
                s = v(i,j);
                v(i,j) = v(j,i);
                v(j,i) = s;
            }
        }
        svdfit(n, vsmall, v, d);
        if( scbd>1 )
        {
            for(i = 1; i <= n; i++)
            {
                s = z(i);
                for(j = 1; j <= n; j++)
                {
                    v(i,j) = s*v(i,j);
                }
            }
            for(i = 1; i <= n; i++)
            {
                s = 0;
                for(j = 1; j <= n; j++)
                {
                    s = s+ap::sqr(v(j,i));
                }
                s = sqrt(s);
                d(i) = s*d(i);
                s = 1/s;
                for(j = 1; j <= n; j++)
                {
                    v(j,i) = s*v(j,i);
                }
            }
        }
        for(i = 1; i <= n; i++)
        {
            dni = dn*d(i);
            if( dni>large )
            {
                d(i) = vsmall;
            }
            else
            {
                if( dni<small )
                {
                    d(i) = vlarge;
                }
                else
                {
                    d(i) = 1/(dni*dni);
                }
            }
        }
        svdsort(n, d, v);
        dmin = d(n);
        if( dmin<small )
        {
            dmin = small;
        }
        illc = false;
        if( m2*d(1)>dmin )
        {
            illc = true;
        }
    }
    return result;
}


/*************************************************************************
Служебная подпрограмма SVD-разложения.

Улучшенная версия, адаптированная под данную задачу.
*************************************************************************/
static void svdfit(const int& n,
     const double& tol,
     ap::real_2d_array& ab,
     ap::real_1d_array& q)
{
    int i;
    int j;
    int k;
    int ii;
    int kk;
    int ll2;
    int l;
    int kt;
    int lp1;
    int l2;
    double af;
    double c;
    double g;
    double s;
    double h;
    double x;
    double y;
    double z;
    double temp;
    ap::real_1d_array e;
    double eps;
    int flag1;

    e.setbounds(1, n);
    if( n==1 )
    {
        q(1) = ab(1,1);
        ab(1,1) = 1;
        return;
    }
    eps = ap::machineepsilon;
    g = 0;
    x = 0;
    for(i = 1; i <= n; i++)
    {
        e(i) = g;
        s = 0;
        l = i+1;
        for(j = i; j <= n; j++)
        {
            s = s+ap::sqr(ab(j,i));
        }
        g = 0;
        if( s>=tol )
        {
            af = ab(i,i);
            g = sqrt(s);
            if( af>=0 )
            {
                g = -g;
            }
            h = af*g-s;
            ab(i,i) = af-g;
            if( l<=n )
            {
                for(j = l; j <= n; j++)
                {
                    af = 0;
                    for(k = i; k <= n; k++)
                    {
                        af = af+ab(k,i)*ab(k,j);
                    }
                    af = af/h;
                    for(k = i; k <= n; k++)
                    {
                        ab(k,j) = ab(k,j)+af*ab(k,i);
                    }
                }
            }
        }
        q(i) = g;
        s = 0;
        if( i!=n )
        {
            for(j = l; j <= n; j++)
            {
                s = s+ab(i,j)*ab(i,j);
            }
        }
        g = 0;
        if( s>=tol )
        {
            if( i!=n )
            {
                af = ab(i,i+1);
            }
            g = sqrt(s);
            if( af>=0 )
            {
                g = -g;
            }
            h = af*g-s;
            if( i!=n )
            {
                ab(i,i+1) = af-g;
                for(j = l; j <= n; j++)
                {
                    e(j) = ab(i,j)/h;
                }
                for(j = l; j <= n; j++)
                {
                    s = 0;
                    for(k = l; k <= n; k++)
                    {
                        s = s+ab(j,k)*ab(i,k);
                    }
                    for(k = l; k <= n; k++)
                    {
                        ab(j,k) = ab(j,k)+s*e(k);
                    }
                }
            }
        }
        y = fabs(q(i))+fabs(e(i));
        if( y>x )
        {
            x = y;
        }
    }
    ab(n,n) = 1;
    g = e(n);
    l = n;
    for(ii = 2; ii <= n; ii++)
    {
        i = n-ii+1;
        if( g!=0 )
        {
            h = ab(i,i+1)*g;
            for(j = l; j <= n; j++)
            {
                ab(j,i) = ab(i,j)/h;
            }
            for(j = l; j <= n; j++)
            {
                s = 0;
                for(k = l; k <= n; k++)
                {
                    s = s+ab(i,k)*ab(k,j);
                }
                for(k = l; k <= n; k++)
                {
                    ab(k,j) = ab(k,j)+s*ab(k,i);
                }
            }
        }
        for(j = l; j <= n; j++)
        {
            ab(i,j) = 0;
            ab(j,i) = 0;
        }
        ab(i,i) = 1;
        g = e(i);
        l = i;
    }
    eps = eps*x;
    for(kk = 1; kk <= n; kk++)
    {
        k = n-kk+1;
        kt = 0;
        while(true)
        {
            kt = kt+1;
            if( kt>30 )
            {
                e(k) = 0;
            }
            flag1 = 110;
            for(ll2 = 1; ll2 <= k; ll2++)
            {
                l2 = k-ll2+1;
                l = l2;
                if( fabs(e(l))<eps )
                {
                    flag1 = 120;
                    break;
                }
                if( l==1 )
                {
                    continue;
                }
                if( fabs(q(l-1))<=eps )
                {
                    break;
                }
            }
            if( flag1!=120 )
            {
                c = 0;
                s = 1;
                for(i = l; i <= k; i++)
                {
                    af = s*e(i);
                    e(i) = c*e(i);
                    if( fabs(af)<=eps )
                    {
                        break;
                    }
                    g = q(i);
                    if( fabs(af)<fabs(g) )
                    {
                        h = fabs(g)*sqrt(1+ap::sqr(af/g));
                    }
                    else
                    {
                        if( af==0 )
                        {
                            h = 0;
                        }
                        else
                        {
                            h = fabs(af)*sqrt(1+ap::sqr(g/af));
                        }
                    }
                    q(i) = h;
                    if( h==0 )
                    {
                        g = 1;
                        h = 1;
                    }
                    c = g/h;
                    s = -af/h;
                }
            }
            z = q(k);
            if( l==k )
            {
                break;
            }
            x = q(l);
            y = q(k-1);
            g = e(k-1);
            h = e(k);
            af = ((y-z)*(y+z)+(g-h)*(g+h))/(2*h*y);
            g = sqrt(af*af+1);
            temp = af-g;
            if( af>=0 )
            {
                temp = af+g;
            }
            af = ((x-z)*(x+z)+h*(y/temp-h))/x;
            c = 1;
            s = 1;
            lp1 = l+1;
            if( lp1<=k )
            {
                for(i = lp1; i <= k; i++)
                {
                    g = e(i);
                    y = q(i);
                    h = s*g;
                    g = g*c;
                    if( fabs(af)<fabs(h) )
                    {
                        z = fabs(h)*sqrt(1+ap::sqr(af/h));
                    }
                    else
                    {
                        if( af==0 )
                        {
                            z = 0;
                        }
                        else
                        {
                            z = fabs(af)*sqrt(1+ap::sqr(h/af));
                        }
                    }
                    e(i-1) = z;
                    if( z==0 )
                    {
                        af = 1;
                        z = 1;
                    }
                    c = af/z;
                    s = h/z;
                    af = x*c+g*s;
                    g = -x*s+g*c;
                    h = y*s;
                    y = y*c;
                    for(j = 1; j <= n; j++)
                    {
                        x = ab(j,i-1);
                        z = ab(j,i);
                        ab(j,i-1) = x*c+z*s;
                        ab(j,i) = -x*s+z*c;
                    }
                    if( fabs(af)<fabs(h) )
                    {
                        z = fabs(h)*sqrt(1+ap::sqr(af/h));
                    }
                    else
                    {
                        if( af==0 )
                        {
                            z = 0;
                        }
                        else
                        {
                            z = fabs(af)*sqrt(1+ap::sqr(h/af));
                        }
                    }
                    q(i-1) = z;
                    if( z==0 )
                    {
                        af = 1;
                        z = 1;
                    }
                    c = af/z;
                    s = h/z;
                    af = c*g+s*y;
                    x = -s*g+c*y;
                }
            }
            e(l) = 0;
            e(k) = af;
            q(k) = x;
        }
        if( z>=0 )
        {
            continue;
        }
        q(k) = -z;
        for(j = 1; j <= n; j++)
        {
            ab(j,k) = -ab(j,k);
        }
    }
}


/*************************************************************************
Подпрограмма минимизации функции вдоль заданного направления или вдоль
кривой второго порядка. Далее - выдержка из оригинального комментария.

...THE SUBROUTINE MIN MINIMIZES F FROM X IN THE DIRECTION V(*,J) UNLESS
   J IS LESS THAN 1, WHEN A QUADRATIC SEARCH IS MADE IN THE PLANE
   DEFINED BY Q0,Q1,X.
   D2 IS EITHER ZERO OR AN APPROXIMATION TO HALF F".
   ON ENTRY, X1 IS AN ESTIMATE OF THE DISTANCE FROM X TO THE MINIMUM
   ALONG V(*,J) (OR, IF J=0, A CURVE).  ON RETURN, X1 IS THE DISTANCE
   FOUND.
   IF FK=.TRUE., THEN F1 IS FLIN(X1).  OTHERWISE X1 AND F1 ARE IGNORED
   ON ENTRY UNLESS FINAL FX IS GREATER THAN F1.
   NITS CONTROLS THE NUMBER OF TIMES AN ATTEMPT WILL BE MADE TO HALVE
   THE INTERVAL.
*************************************************************************/
static void directionalminimize(const int& n,
     const int& j,
     const int& nits,
     double& d2,
     double& x1,
     double& f1,
     const bool& fk,
     ap::real_1d_array& x,
     double& t,
     const double& machep,
     const double& h)
{
    int i;
    int k;
    double sf1;
    double sx1;
    double xm;
    double fm;
    double f0;
    double s;
    double temp;
    double t2;
    double f2;
    double x2;
    double d1;
    bool dz;
    double m2;
    double m4;
    double small;
    int cycleflag;

    small = ap::sqr(machep);
    m2 = sqrt(machep);
    m4 = sqrt(m2);
    sf1 = f1;
    sx1 = x1;
    k = 0;
    xm = 0;
    fm = fx;
    f0 = fx;
    dz = d2<machep;
    s = 0;
    for(i = 1; i <= n; i++)
    {
        s = s+ap::sqr(x(i));
    }
    s = sqrt(s);
    temp = d2;
    if( dz )
    {
        temp = dmin;
    }
    t2 = m4*sqrt(fabs(fx)/temp+s*ldt)+m2*ldt;
    s = m4*s+t;
    if( dz&&t2>s )
    {
        t2 = s;
    }
    if( t2<small )
    {
        t = small;
    }
    if( t2>0.01*h )
    {
        t2 = 0.01*h;
    }
    if( fk&&f1<=fm )
    {
        xm = x1;
        fm = f1;
    }
    if( !fk||fabs(x1)<t2 )
    {
        temp = 1;
        if( x1<0 )
        {
            temp = -1;
        }
        x1 = temp*t2;
        f1 = flin(n, j, x1, x);
    }
    if( f1<=fm )
    {
        xm = x1;
        fm = f1;
    }
    cycleflag = 4;
    while(true)
    {
        if( cycleflag==4 )
        {
            if( dz )
            {
                x2 = -x1;
                if( f0>=f1 )
                {
                    x2 = 2*x1;
                }
                f2 = flin(n, j, x2, x);
                if( f2<=fm )
                {
                    xm = x2;
                    fm = f2;
                }
                d2 = (x2*(f1-f0)-x1*(f2-f0))/(x1*x2*(x1-x2));
            }
            d1 = (f1-f0)/x1-x1*d2;
            dz = true;
            if( d2>small )
            {
                x2 = (-0.5*d1)/d2;
            }
            else
            {
                x2 = h;
                if( d1>=0 )
                {
                    x2 = -x2;
                }
            }
            if( fabs(x2)>h )
            {
                if( x2<=0 )
                {
                    x2 = -h;
                }
                else
                {
                    x2 = h;
                }
            }
        }
        f2 = flin(n, j, x2, x);
        if( k>=nits||f2<=f0 )
        {
            break;
        }
        k = k+1;
        if( f0<f1&&x1*x2>0 )
        {
            cycleflag = 4;
            continue;
        }
        x2 = 0.5*x2;
        cycleflag = 11;
    }
    nl = nl+1;
    if( f2<=fm )
    {
        fm = f2;
    }
    else
    {
        x2 = xm;
    }
    if( fabs(x2*(x2-x1))<=small )
    {
        if( k>0 )
        {
            d2 = 0;
        }
    }
    else
    {
        d2 = (x2*(f1-f0)-x1*(fm-f0))/(x1*x2*(x1-x2));
    }
    if( d2<=small )
    {
        d2 = small;
    }
    x1 = x2;
    fx = fm;
    if( sf1<fx )
    {
        fx = sf1;
        x1 = sx1;
    }
    if( j==0 )
    {
        return;
    }
    for(i = 1; i <= n; i++)
    {
        x(i) = x(i)+x1*v(i,j);
    }
}


/*************************************************************************
Функция одной переменной, минимизируемая подпрограммой DirectionalMinimize.


...FLIN IS THE FUNCTION OF ONE REAL VARIABLE L THAT IS MINIMIZED
   BY THE SUBROUTINE DirectionalMinimize...
*************************************************************************/
static double flin(const int& n,
     const int& j,
     const double& l,
     const ap::real_1d_array& x)
{
    double result;
    ap::real_1d_array t;
    int i;

    t.setbounds(1, n);
    if( j!=0 )
    {
        for(i = 1; i <= n; i++)
        {
            t(i) = x(i)+l*v(i,j);
        }
    }
    else
    {
        qa = l*(l-qd1)/(qd0*(qd0+qd1));
        qb = (l+qd0)*(qd1-l)/(qd0*qd1);
        qc = l*(l+qd0)/(qd1*(qd0+qd1));
        for(i = 1; i <= n; i++)
        {
            t(i) = qa*q0(i)+qb*x(i)+qc*q1(i);
        }
    }
    nf = nf+1;
    result = f(t);
    return result;
}


/*************************************************************************
Подпрограмма сортировки столбцов матрицы главных направлений в соответствии
с их сингулярными значениями
*************************************************************************/
static void svdsort(const int& n,
     ap::real_1d_array& vecd,
     ap::real_2d_array& matv)
{
    double s;
    int i;
    int j;
    int k;
    int nm1;

    if( n==1 )
    {
        return;
    }
    nm1 = n-1;
    for(i = 1; i <= nm1; i++)
    {
        k = i;
        s = vecd(i);
        for(j = i+1; j <= n; j++)
        {
            if( vecd(j)<=s )
            {
                continue;
            }
            k = j;
            s = vecd(j);
        }
        if( k<=i )
        {
            continue;
        }
        vecd(k) = vecd(i);
        vecd(i) = s;
        for(j = 1; j <= n; j++)
        {
            s = matv(j,i);
            matv(j,i) = matv(j,k);
            matv(j,k) = s;
        }
    }
}


/*************************************************************************
...QUAD LOOKS FOR THE MINIMUM OF F ALONG A CURVE DEFINED BY Q0,Q1,X...
*************************************************************************/
static void quadsearch(const int& n,
     ap::real_1d_array& x,
     double& t,
     const double& machep,
     const double& h)
{
    int i;
    double s;
    double value;
    double l;

    s = fx;
    fx = qf1;
    qf1 = s;
    qd1 = 0;
    for(i = 1; i <= n; i++)
    {
        s = x(i);
        l = q1(i);
        x(i) = l;
        q1(i) = s;
        qd1 = qd1+ap::sqr(s-l);
    }
    qd1 = sqrt(qd1);
    l = qd1;
    s = 0;
    if( qd0<=0||qd1<=0||nl<3*n*n )
    {
        fx = qf1;
        qa = 0;
        qb = qa;
        qc = 1;
    }
    else
    {
        value = qf1;
        directionalminimize(n, 0, 2, s, l, value, true, x, t, machep, h);
        qa = l*(l-qd1)/(qd0*(qd0+qd1));
        qb = (l+qd0)*(qd1-l)/(qd0*qd1);
        qc = l*(l+qd0)/(qd1*(qd0+qd1));
    }
    qd0 = qd1;
    for(i = 1; i <= n; i++)
    {
        s = q0(i);
        q0(i) = x(i);
        x(i) = qa*s+qb*x(i)+qc*q1(i);
    }
}



