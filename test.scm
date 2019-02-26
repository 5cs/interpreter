> (define square (lambda (x) (* x x)))
=> nil
> (define average (lambda (x y) (/ (+ x y) 2)))
=> nil
> (define abs (lambda (x) (if (< x 0) (- 0 x) x)))
=> nil
> (define good-enough? (lambda (guess x) (< (abs (- (square guess) x)) 0.001)))
=> nil
> (define improve (lambda (guess x) (average guess (/ x guess))))
=> nil
> (define sqrt-iter (lambda (guess x) (if (good-enough? guess x) guess (sqrt-iter (improve guess x) x))))
=> nil
> (define sqrt (lambda (x) (sqrt-iter 1.0 x)))
=> nil
> (sqrt 4)
> (define sqrt-iter (lambda (guess x) (if (< (abs (- (square guess) x)) 0.001) guess (sqrt-iter (improve guess x) x))))