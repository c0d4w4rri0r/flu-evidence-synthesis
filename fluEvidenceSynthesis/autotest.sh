#!/bin/bash

# Always run at least once
R -e 'Rcpp::compileAttributes(".",verbose=TRUE)';
echo -e "#include \"rcppwrap.hh\"\n$(cat src/RcppExports.cpp)" > src/RcppExports.cpp;
R CMD check ../fluEvidenceSynthesis

# This script depends on inotify-hookable
inotify-hookable -q -w data -w src -w tests -f DESCRIPTION -c "R -e 'Rcpp::compileAttributes(\".\",verbose=TRUE)'; echo -e "#include \"rcppwrap.hh\"\n$(cat src/RcppExports.cpp)" > src/RcppExports.cpp; R CMD check ../fluEvidenceSynthesis"