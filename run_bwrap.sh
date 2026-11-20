#!/usr/bin/env bash
export WORKSPACE="${WORKSPACE:-/home/gustavo/Documentos/USP/IC/research/software-testing}"
export D4J_BIN_DIR="/home/gustavo/Documentos/USP/IC/research/defects4j/framework/bin"
export PERL5LIB="/home/gustavo/perl5/lib/perl5"
bwrap \
  --die-with-parent \
  --new-session \
  --unshare-pid \
  --unshare-ipc \
  --unshare-uts \
  --ro-bind /usr /usr \
  --ro-bind /lib /lib \
  --ro-bind /lib64 /lib64 \
  --ro-bind /bin /bin \
  --ro-bind /etc /etc \
  --ro-bind /home/gustavo/Documentos/USP/IC/research/defects4j \
  /home/gustavo/Documentos/USP/IC/research/defects4j \
  --ro-bind /home/gustavo/perl5 /home/gustavo/perl5 \
  --ro-bind /home/gustavo/Documentos/USP/IC/research/software-testing/build/swt /swt \
  --bind "$WORKSPACE" "$WORKSPACE" \
  --tmpfs /tmp \
  --proc /proc \
  --dev /dev \
  --chdir "$WORKSPACE" \
  --clearenv \
  --setenv PATH "$D4J_BIN_DIR:/usr/bin:/bin" \
  --setenv PERL5LIB "$PERL5LIB" \
  --setenv HOME /tmp \
  --setenv TMPDIR /tmp \
  /swt "$@"
