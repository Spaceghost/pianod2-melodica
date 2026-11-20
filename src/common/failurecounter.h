///
/// A class for counting and reporting test failures.
/// @file       failurecounter.h - Testing utility
/// @author     Perette Barella
/// @date       2019-05-01
/// @copyright  Copyright 2019-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <iostream>
#include <sstream>

#ifdef HAVE_NCURSES
#include <curses.h>
#include <term.h>
#endif

namespace Test {

    class FailureCounter {
    private:
        static int grand_total_tests;
        static int grand_total_passes;
        static int grand_total_errors;
        static int grand_total_warnings;
        static int grand_total_skips;
        static std::string green;
        static std::string red;
        static std::string yellow;
        static std::string bold;
        static std::string normal;
        std::string name;
        std::string subtest_name;
        int tests{0};
        int passes{0};
        int errors{0};
        int warnings{0};
        int skips{0};
        int check_number{0};

    public:
        /** If ncurses is available, initializes terminal color escape sequences.
            @warning Calling ncurses' `setupterm` function, as done by this
            function, interferes with GDB breakpoints.  Don't enable colors
            if debugging. */
        static void initialize_colors() {
#ifdef HAVE_NCURSES
            setupterm (NULL, 1, NULL);
            const char *setcolor = tigetstr ("setaf");
            green = setcolor ? tiparm (setcolor, 2) : "";
            red = setcolor ? tiparm (setcolor, 1) : "";
            yellow = setcolor ? tiparm (setcolor, 3) : "";
            const char *temp = tigetstr ("sgr0");
            normal = temp ? temp : "";
            temp = tigetstr ("bold");
            bold = temp ? temp : "";
#else
            std::cerr << "Not compiled with ncurses; colors not available\n";
#endif
        }

        /** Start a new group of tests.
            @param n The name of the test group */
        FailureCounter (const std::string &n) : name (n) {
            std::cerr << "==============================================\n"
                         "Performing test "
                      << bold << name << normal << std::endl
                      << "----------------------------------------------\n\n";
        }

        /** On destruction, report summary of test results. */
        ~FailureCounter() {
            std::cerr << std::endl << "Test group " << name << std::endl << tests << " tests run: ";
            grand_total_tests += tests;
            grand_total_passes += passes;
            grand_total_errors += errors;
            grand_total_warnings += warnings;
            grand_total_skips += skips;
            assert (tests == passes + errors + warnings + skips);
            if (errors) {
                std::cerr << errors << red << " FAILURES, " << normal;
            }
            std::cerr << passes << green << " passed" << normal;
            if (warnings) {
                std::cerr << ", " << warnings << yellow << " warnings" << normal;
            }
            if (skips) {
                std::cerr << ", " << skips << yellow << " tests skipped" << normal;
            }
            std::cerr << std::endl << std::endl;
        }

        /** Provide a name for a group of subtests.
            @param n The subtest group name. */
        void subtest (const std::string &n) {
            subtest_name = n;
            check_number = 0;
            std::cerr << std::endl << "=== Testing " << name << " / " << subtest_name << " ===" << std::endl;
        }

        /** Record that a test was skipped.
            @param reason The reason for skipping. */
        void skip (const std::string &reason) {
            check_number++;
            tests++;
            skips++;
            std::cout << yellow << "skip: " << normal << reason << std::endl;
        }

        /** Record that a test generated a warning.
            @param reason The reason for the warning. */
        void warning (const std::string &reason) {
            check_number++;
            tests++;
            warnings++;
            std::cout << yellow << "warning: " << normal << reason << std::endl;
        }

        /** Record and report a successful test.
            @param success Name or details of the success */
        bool succeed (const std::string &success) {
            check_number++;
            tests++;
            passes++;
            std::cout << green << "ok: " << normal << success << std::endl;
            return true;
        }

        /** Record and report a failed test.
            @param failure The name or reason for the failure.
            @param detail Additional details about the failure. */
        bool fail (const std::string &failure, const std::string &detail = "") {
            check_number++;
            tests++;
            errors++;
            std::cout << red << name << (subtest_name.empty() ? "" : " / ") << subtest_name << " check #"
                      << check_number << " error:" << normal << std::endl;
            std::cout << failure << (detail.empty() ? "" : ": ") << detail << std::endl;
            return false;
        }

        bool result (bool result, const std::string &test, const std::string &detail = "") {
            if (result) {
                return succeed (test);
            } else {
                return fail (test, detail);
            }
        }

        template <typename T>
        bool equal (const std::string &test_name, const T &expect, const T &actual) {
            if (result (expect == actual, test_name, "mismatch:")) {
                return true;
            }
            std::cout << "mismatch- expect: '" << expect << '\'' << std::endl;
            std::cout << "          actual: '" << actual << '\'' << std::endl;
            return false;
        }

        bool equal (const std::string &test_name, const char *expect, const char *actual) {
            return equal (test_name, std::string{expect}, std::string{actual});
        }

        bool equal (const std::string &test_name, const char *expect, const std::string &actual) {
            return equal (test_name, std::string{expect}, actual);
        }

        bool equal (const std::string &test_name, long expect, const long actual) {
            return equal<long> (test_name, expect, actual);
        }

        bool isTrue (const std::string &test_name, bool actual) {
            return equal (test_name, "true", actual ? "true" : "false");
        }

        bool isFalse (const std::string &test_name, bool actual) {
            return equal (test_name, "false", actual ? "true" : "false");
        }

        bool signEqual (const std::string &test_name, long expect, long actual) {
            return equal (test_name, expect < 0 ? -1 : expect > 0 ? 1 : 0, actual < 0 ? -1 : actual > 0 ? 1 : 0);
        }

        /*
         *              2-parameter checks
         */
        template <typename T>
        bool equal (const T &expect, const T &actual) {
            if (expect == actual) {
                std::stringstream name;
                name << expect;
                return succeed (name.str());
            }
            fail ("mismatch:");
            std::cout << "mismatch- expect: '" << expect << '\'' << std::endl;
            std::cout << "          actual: '" << actual << '\'' << std::endl;
            return false;
        }

        bool equal (const char *expect, const char *actual) {
            return equal (std::string{expect}, std::string{actual});
        }

        bool equal (const char *expect, const std::string &actual) {
            return equal (std::string{expect}, actual);
        }

        bool equal (long expect, const long actual) {
            return equal<long> (expect, actual);
        }

        bool isTrue (bool actual) {
            return equal ("true", actual ? "true" : "false");
        }

        bool isFalse (bool actual) {
            return equal ("false", actual ? "true" : "false");
        }

        /*
         *              Specialized checks
         */
        bool sameException (const std::string &trigger,
                            const std::exception &expect,
                            const std::exception &actual,
                            bool compare_text = false) {
            if (typeid (expect) == typeid (actual)) {
                if (compare_text && (strcmp (expect.what(), actual.what()) != 0)) {
                    fail ("Wrong exception text", trigger);
                    std::cerr << "mismatch- expect: " << expect.what() << std::endl;
                    std::cerr << "          actual: " << actual.what() << std::endl;
                    return false;
                }
                succeed (trigger + ": " + expect.what());
                return true;
            }
            fail ("Wrong exception", trigger);
            std::cerr << "mismatch- expect: " << typeid (expect).name() << std::endl;
            std::cerr << "          actual: " << typeid (actual).name() << std::endl;
            return false;
        }

        bool sameExceptionAndText (const std::string &trigger,
                                   const std::exception &expect,
                                   const std::exception &actual) {
            return sameException (trigger, expect, actual, true);
        }

        inline int errorCount() {
            return errors;
        }

        static inline int grandTotalErrors() {
            return grand_total_errors;
        }

        static inline void summary() {
            std::ostringstream sum;
            if (grand_total_errors == 0) {
                std::cerr << "All tests passed!" << std::endl << std::endl;
                ;
            } else {
                std::cerr << "Some tests FAILED!" << normal << std::endl << std::endl;
            }
            std::cerr << "Tests performed:         " << grand_total_tests << std::endl;
            std::cerr << "Tests passed:            " << green << grand_total_passes << normal << std::endl;
            if (grand_total_errors) {
                std::cerr << "Tests failed:            " << red << grand_total_errors << normal << std::endl;
            }
            if (grand_total_warnings) {
                std::cerr << "Warnings generated:      " << yellow << grand_total_warnings << normal << std::endl;
            }
            if (grand_total_skips) {
                std::cerr << "Tests skipped            " << yellow << grand_total_skips << normal << std::endl;
            }
        }
    };
    std::string FailureCounter::green;
    std::string FailureCounter::red;
    std::string FailureCounter::yellow;
    std::string FailureCounter::bold;
    std::string FailureCounter::normal;
    int FailureCounter::grand_total_tests;
    int FailureCounter::grand_total_passes;
    int FailureCounter::grand_total_errors;
    int FailureCounter::grand_total_warnings;
    int FailureCounter::grand_total_skips;

}  // namespace Test
