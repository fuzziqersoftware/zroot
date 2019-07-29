.intel_syntax noprefix


complex_mul:
  # overall this is:
  #   ret.real = (a.real * b.real) - (a.imag * b.imag)
  #   ret.imag = (a.imag * b.real) + (a.real * b.imag)
  # takes xmm0=a, xmm1=b;  ret is returned in xmm0 real:imag (high:low)
  movapd    xmm6, xmm0
  mulpd     xmm6, xmm1
  hsubpd    xmm6, xmm6
  movapd    xmm7, xmm0
  shufpd    xmm7, xmm7, 1
  mulpd     xmm7, xmm1
  haddpd    xmm7, xmm7
  movapd    xmm0, xmm7
  movsd     xmm0, xmm6
  ret

# void complex_mul_asm(complex* result, const complex* a, const complex* b)
complex_div:
  # takes xmm0=a, xmm1=b;  ret is returned in xmm0 real:imag (high:low)

  # ((ar * br + ai + bi) / denom) + ((ai * br - ar * bi) / denom) i

  # xmm0 = a.real : a.imag
  # xmm1 = b.real : b.imag

  movapd    xmm7, xmm1
  mulpd     xmm7, xmm7
  haddpd    xmm7, xmm7
  # xmm7 = b.real * b.real + b.imag * b.imag : b.real * b.real + b.imag * b.imag
  # xmm7 = denom : denom

  movapd    xmm6, xmm0
  mulpd     xmm6, xmm1
  haddpd    xmm6, xmm6
  # xmm6 = a.real * b.real + a.imag * b.imag : a.real * b.real + a.imag * b.imag

  movapd    xmm5, xmm0
  shufpd    xmm5, xmm5, 1
  mulpd     xmm5, xmm1
  hsubpd    xmm5, xmm5
  # xmm5 = a.imag * b.real - a.real * b.imag : a.imag * b.real - a.real * b.imag

  movapd    xmm0, xmm5
  movsd     xmm0, xmm6
  divpd     xmm0, xmm7
  # xmm0 = (ar * br + ai * bi) / denom : (ai * br - ar * bi) / denom

  ret


root_iterate:
  # NOTE: this function must be called unaligned
  # rdi = coeffs ptr
  # rsi = coeffs count
  # xmm0 = guess
  # returns xmm0 = result

  # must be at least 2 coefficients; if not, fail
  cmp       rsi, 2
  jge       root_iterate_enough_coeffs
  xorpd     xmm0, xmm0
  ret

root_iterate_enough_coeffs:
  # allocate guess_powers array on stack
  mov       rcx, rsi
  imul      rcx, 0x10
  sub       rsp, rcx

  # set guess_powers[0] to 1 + 0i
  xor       rax, rax
  mov       [rsp + 0x08], rax
  mov       ax, 0x3FF0
  shl       rax, 48
  mov       [rsp], rax

  # put guess in xmm1, guess_powers[x] in xmm0
  movapd    xmm1, xmm0
  movapd    xmm0, [rsp]

  # compute guess powers beyond 1 by repeatedly multiplying by guess.
  # conveniently, complex_mul doesn't modify xmm1 so we can leave guess there
  lea       r9, [rsp + rcx]
  lea       r8, [rsp + 0x10]
root_iterate_compute_next_guess_power:
  call      complex_mul
  movapd    [r8], xmm0
  add       r8, 0x10
  cmp       r8, r9
  jl        root_iterate_compute_next_guess_power

  # at this point, the data we have is:
  #   rdi = coeffs ptr
  #   rsi = coeffs count
  #   xmm1 = guess
  #   [rsp + 0x00] = guess_powers[0]
  #   [rsp + 0x10] = guess_powers[1]
  #   [rsp + ....]

  # rdx = degree (== coeffs count - 1)
  lea       rdx, [rsi - 1]

  # make room for numer (xmm4) and denom (xmm5)
  xorpd     xmm4, xmm4
  xorpd     xmm5, xmm5

  xor       r8, r8  # x
  xor       r9, r9  # x * 0x10 (for offset into coeffs)
  lea       r10, [rcx - 0x10]  # (degree - x) * 0x10 (for offset into guess_powers)
root_iterate_update_numer_denom_again:

  # xmm0 = guess_powers[degree - x] * coeffs[x]
  movapd    xmm1, [rdi + r9]   # xmm1 = coeffs[x]
  movapd    xmm0, [rsp + r10]  # xmm0 = guess_powers[degree - x]
  call      complex_mul

  # xmm0 = (guess_powers[degree - x] * coeffs[x]) * (degree - x - 1)
  lea       r11, [rdx - 1]
  sub       r11, r8
  cvtsi2sd  xmm2, r11
  movddup   xmm2, xmm2
  mulpd     xmm0, xmm2

  # numer = numer + (guess_powers[degree - x] * coeffs[x]) * (degree - x - 1)
  addpd     xmm4, xmm0

  # xmm0 = guess_powers[degree - x - 1] * coeffs[x]
  # note that xmm1 is still coeffs[x]
  movapd    xmm0, [rsp + r10 - 0x10]  # xmm0 = guess_powers[degree - x - 1]
  call      complex_mul

  # xmm0 = (guess_powers[degree - x - 1] * coeffs[x]) * (degree - x)
  inc       r11  # r11 was already (degree - x - 1)
  cvtsi2sd  xmm2, r11
  movddup   xmm2, xmm2
  mulpd     xmm0, xmm2

  # denom = denom + (guess_powers[degree - x - 1] * coeffs[x]) * (degree - x)
  addpd     xmm5, xmm0

  # go through the loop again
  add       r9, 0x10
  sub       r10, 0x10
  inc       r8
  cmp       r8, rsi
  jl        root_iterate_update_numer_denom_again

  # return (numer / denom)
  movapd    xmm0, xmm4
  movapd    xmm1, xmm5
  add       rsp, rcx
  jmp       complex_div


# void root_iterate(const complex* coeffs, size_t coeffs_count,
#     const complex* guess, complex* result)
.globl _root_iterate_asm
_root_iterate_asm:
  push      rdx
  push      rcx
  movupd    xmm0, [rdx]
  call      root_iterate
  pop       rcx
  pop       rdx
  movupd    [rcx], xmm0
  ret


# void root_asm(const complex* coeffs, size_t coeffs_count,
#     const complex* guess, double precision, size_t* max, complex* result);
.globl _root_asm
_root_asm:
  # rdi = coeffs ptr
  # rsi = coeffs count
  # rdx = guess ptr
  # rcx = max ptr
  # r8 = result ptr
  # xmm0 = precision

  sub       rsp, 0x10
  push      r8
  push      rcx
  movsd     xmm3, xmm0
  movupd    xmm0, [rdx]

  # now we have this information:
  #   xmm0 = this_guess
  #   xmm3 = precision
  #   rdi = coeffs ptr (will be passed directly to root_iterate)
  #   rsi = coeffs count (will be passed directly to root_iterate)
  #   [rsp] = max ptr
  #   [rsp + 0x08] = result ptr
  #   [rsp + 0x10] = last (uninitialized)

root_again:
  movapd    [rsp + 0x10], xmm0
  call      root_iterate

  movapd    xmm1, [rsp + 0x10]
  # TODO: implement the inexact equal check here (note that xmm3 is unused by
  # the called functions, so it still contains precision)

  mov       rax, [rsp]
  dec       qword ptr [rax]
  jnz       root_again

  # TODO: return *max ? this_guess : zero;
