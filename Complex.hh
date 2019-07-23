#pragma once


struct complex {
  double real;
  double imag;

  complex();
  complex(double, double);

  inline bool operator==(const complex& b) const {
    return (this->real == b.real) && (this->imag == b.imag);
  }
  inline bool operator!=(const complex& b) const {
    return (this->real != b.real) || (this->imag != b.imag);
  }

  inline complex operator+(const complex& b) const {
    return complex(this->real + b.real, this->imag + b.imag);
  }
  inline complex& operator+=(const complex& b) {
    this->real += b.real;
    this->imag += b.imag;
    return *this;
  }

  inline complex operator-(const complex& b) const {
    return complex(this->real - b.real, this->imag - b.imag);
  }
  inline complex& operator-=(const complex& b) {
    this->real -= b.real;
    this->imag -= b.imag;
    return *this;
  }

  inline complex operator*(const complex& b) const {
    return complex((this->real * b.real) - (this->imag * b.imag),
        (this->imag * b.real) + (this->real * b.imag));
  }
  inline complex& operator*=(const complex& b) {
    double new_real = (this->real * b.real) - (this->imag * b.imag);
    this->imag = (this->imag * b.real) + (this->real * b.imag);
    this->real = new_real;
    return *this;
  }

  inline complex operator/(const complex& b) const {
    double denom = (b.real * b.real) + (b.imag * b.imag);
    return complex((((this->real * b.real) + (this->imag * b.imag)) / denom),
        (((this->imag * b.real) - (this->real * b.imag)) / denom));
  }
  inline complex operator/=(const complex& b) {
    double denom = (b.real * b.real) + (b.imag * b.imag);
    double new_real = ((this->real * b.real) + (this->imag * b.imag)) / denom;
    this->imag = ((this->imag * b.real) - (this->real * b.imag)) / denom;
    this->real = new_real;
    return *this;
  }

  inline complex operator*(double b) const {
    return complex(this->real * b, this->imag * b);
  }
  inline complex operator*=(double b) {
    this->real *= b;
    this->imag *= b;
    return *this;
  }

  inline complex operator/(double b) const {
    return complex(this->real / b, this->imag / b);
  }
  inline complex operator/=(double b) {
    this->real /= b;
    this->imag /= b;
    return *this;
  }

  complex pow(int p) const;
  bool equal(const complex& b, double precision) const;

  inline double abs2() const {
    return (this->real * this->real) + (this->imag * this->imag);
  }
};
