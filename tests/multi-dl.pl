if ($ENV{NO_MULTIDL}) {
    print STDERR "SKIP: No multi download.\n";
    exit 0;
}
exit ccliveTest::run(
    "http://www.youtube.com/watch?v=DeWsZ2b_pK4",
    "http://break.com/index/beach-tackle-whip-lash.html",
    "--remote", # implies internet requirement
    "-f",
    "3gp",
    "-O",
    "/dev/null"
);