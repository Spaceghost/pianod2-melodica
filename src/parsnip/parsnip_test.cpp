///
/// Parsnip serialization.
/// @file       parsnip_test.cpp - Test program.
/// @author     Perette Barella
/// @date       2019-05-01
/// @copyright  Copyright 2019-2021 Devious Fish. All rights reserved.
///

#include <config.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <typeinfo>

#include <getopt.h>
#include <cstring>

#include "parsnip.h"
#include "parsnip_command.h"
#include "parsnip_schema.h"
#include "parsnip_argv.h"

#include "failurecounter.h"

class FailureCounter : public Test::FailureCounter {
    using Test::FailureCounter::FailureCounter;

public:
    bool equal (const Parsnip::Data &expect, const Parsnip::Data &actual) {
        if (expect == actual) {
            succeed (expect.toJson());
            return true;
        }
        fail ("mismatch:");
        expect.dumpJson ("mismatch- expect");
        actual.dumpJson ("          actual");
        return false;
    }
};

const char *exception_type (const Parsnip::Exception &e) {
    if (typeid (e) == typeid (Parsnip::DataFormatError))
        return ("Xformat");
    if (typeid (e) == typeid (Parsnip::IncorrectDataType))
        return ("XDatatype");
    if (typeid (e) == typeid (Parsnip::DataRangeError))
        return ("XRange");
    if (typeid (e) == typeid (Parsnip::NoSuchKey))
        return ("Xnxkey");
    return ("Xunknown");
}

void render_outputs (const char *name, const Parsnip::Data &datum) {
    std::cout << std::setw (15) << std::left << name << std::setw (10);

    std::cout << (datum.isNull() ? "Null" : "Not-null");

    std::cout << std::setw (10);
    try {
        std::cout << datum.asString();
    } catch (const Parsnip::Exception &e) {
        std::cout << exception_type (e);
    }

    std::cout << std::setw (10);
    try {
        std::cout << datum.asInteger();
    } catch (const Parsnip::Exception &e) {
        std::cout << exception_type (e);
    }

    std::cout << std::setw (10);
    try {
        std::cout << datum.asDouble();
    } catch (const Parsnip::Exception &e) {
        std::cout << exception_type (e);
    }

    try {
        std::cout << (datum.asBoolean() ? "(true)" : "(false)");
    } catch (const Parsnip::Exception &e) {
        std::cout << exception_type (e);
    }
    std::cout << std::endl;
}

void exception_details (const char *name, const Parsnip::Data &datum) {
    try {
        auto junk = datum.asString();
    } catch (const Parsnip::Exception &e) {
        std::cout << std::setw (10) << name << std::setw (12) << "String" << exception_type (e) << ": " << e.what()
                  << std::endl;
    }

    try {
        auto junk = datum.asInteger();
        (void) junk;
    } catch (const Parsnip::Exception &e) {
        std::cout << std::setw (10) << name << std::setw (12) << "Integer" << e.what() << std::endl;
    }

    try {
        auto junk = datum.asDouble();
        (void) junk;
    } catch (const Parsnip::Exception &e) {
        std::cout << std::setw (10) << name << std::setw (12) << "Double" << e.what() << std::endl;
    }

    try {
        auto junk = datum.asBoolean();
        (void) junk;
    } catch (const Parsnip::Exception &e) {
        std::cout << std::setw (10) << name << std::setw (12) << "Boolean" << e.what() << std::endl;
    }

    try {
        auto junk = datum ["bkaayarodoesnotexist"];
    } catch (const Parsnip::Exception &e) {
        std::cout << std::setw (10) << name << std::setw (12) << "Dictionary" << e.what() << std::endl;
    }

    try {
        auto junk = datum [0];
    } catch (const Parsnip::Exception &e) {
        std::cout << std::setw (10) << name << std::setw (12) << "List" << e.what() << std::endl;
    }
}

inline void render_outputs (const std::string &name, const Parsnip::Data &datum) {
    render_outputs (name.c_str(), datum);
}

void render_member (const char *name, const char *field, const Parsnip::Data &datum) {
    try {
        Parsnip::Data item = datum [field];
        render_outputs (name, item);
    } catch (const Parsnip::Exception &e) {
        std::cout << name << ": " << exception_type (e) << std::endl;
    }
}

Parsnip::Data test_copy_construct_and_assign (const Parsnip::Data &data) {
    FailureCounter check (__func__);
    Parsnip::Data copy (data);
    Parsnip::Data five{5};
    Parsnip::Data hello{"Hello"};
    copy = five;
    check.equal (5, copy.asInteger());

    copy = hello;
    check.equal ("Hello", copy.asString());

    check.equal (5, five.asInteger());
    check.isFalse (five.isNull());
    check.equal ("Hello", hello.asString());
    check.isFalse (five.isNull());

    copy = std::move (five);
    check.equal (5, copy.asInteger());
    check.isTrue (five.isNull()); // NOLINT(bugprone-use-after-move)
    five = std::move (copy);

    copy = std::move (hello);
    check.equal ("Hello", copy.asString());
    check.isTrue (hello.isNull()); // NOLINT(bugprone-use-after-move)
    hello = std::move (copy);

    copy = data;
    Parsnip::Data moved (std::move (copy));
    check.isTrue (copy.isNull()); // NOLINT(bugprone-use-after-move)
    copy = moved;
    return copy;
}

void parsnip_data_test() {
    const Parsnip::Data str{"hello"};
    const Parsnip::Data integer{32767};
    const Parsnip::Data boolean{true};
    const Parsnip::Data doub{1.5};

    const Parsnip::Data flex_string{"whale", false};
    const Parsnip::Data flex_integer{"135", false};
    const Parsnip::Data flex_double{"13.5", false};
    const Parsnip::Data flex_boolean{"true", false};
    const Parsnip::Data flex_empty{"", false};
    const Parsnip::Data flex_whitespace{"    ", false};

    const Parsnip::Data nullable0{};
    const Parsnip::Data nullable1{nullptr};

    std::cout << "Field type     Nullable  String    Integer   Double    Boolean" << std::endl;

    render_outputs ("String", str);
    render_outputs ("Integer", integer);
    render_outputs ("Double", doub);
    render_outputs ("Boolean", boolean);
    render_outputs ("Flex(s)", flex_string);
    render_outputs ("Flex(i)", flex_integer);
    render_outputs ("Flex(d)", flex_double);
    render_outputs ("Flex(b)", flex_boolean);
    render_outputs ("Flex() ", flex_empty);
    render_outputs ("Flex( )", flex_whitespace);
    render_outputs ("Null0", nullable0);
    render_outputs ("Null1", nullable1);

    std::cout << std::endl << "Dictionary tests" << std::endl;
    std::cout << "Field type     Nullable  String    Integer   Double    Boolean" << std::endl;
    const Parsnip::Data dict{Parsnip::Data::Dictionary, "foo", 32, "bar", 9.7, "fish", "whale"};
    render_member ("dict[foo]", "foo", dict);
    render_member ("dict[bar]", "bar", dict);
    render_member ("dict[fish]", "fish", dict);
    render_member ("dict[nonexistent]", "nonexistent", dict);
    render_outputs ("dict (raw)", dict);

    std::cout << std::endl << "List tests" << std::endl;
    std::cout << "Field type     Nullable  String    Integer   Double    Boolean" << std::endl;
    const Parsnip::Data list{Parsnip::Data::List, 1, 2, 4, 8.5, 16, 32};
    for (Parsnip::Data::size_type index = 0; index < list.size(); index++) {
        render_outputs ("list[" + std::to_string (index) + "]", list [index]);
    }

    std::cout << "using iterator:" << std::endl;
    for (auto item : test_copy_construct_and_assign (list)) {
        render_outputs ("list it", item);
    }

    std::cout << "using foreach:";
    try {
        auto handler = std::function<void (long)> ([] (long value) -> void { std::cout << ' ' << value; });
        list.foreach (handler);
    } catch (const std::exception &ex) {
        std::cout << std::endl << ex.what() << std::endl;
    }

    std::cout << std::endl << "Copy constructor and assignment tests." << std::endl;
    Parsnip::Data copy = dict;
    assert (copy == dict);
    assert (copy != doub);
    assert (copy != list);
    assert (dict == copy);
    assert (doub != copy);
    assert (list != copy);
    std::cout << "Field type     Nullable  String    Integer   Double    Boolean" << std::endl;
    render_member ("Copy:dict[foo]", "foo", copy);
    copy ["alter"] = "bakayaro";
    assert (copy != dict);
    assert (copy != doub);
    assert (copy != list);
    assert (dict != copy);
    assert (doub != copy);
    assert (list != copy);
    render_member ("copy[alter]", "alter", copy);
    copy = doub;
    assert (copy != dict);
    assert (copy == doub);
    assert (copy != list);
    assert (dict != copy);
    assert (doub == copy);
    assert (list != copy);
    render_outputs ("Copy:Double", copy);
    copy = list;
    assert (copy != dict);
    assert (copy != doub);
    assert (copy == list);
    assert (dict != copy);
    assert (doub != copy);
    assert (list == copy);

    std::cout << "Using asList() iterator:" << std::endl;
    for (auto item : integer.asList()) {
        render_outputs ("list it", item);
    }

    Parsnip::Data complex = Parsnip::Data::make_dictionary (
            {{"integer", integer},
             {"double", doub},
             {"list", list},
             {"dictionary", dict},
             {"escaped", "newline\n return\r backspace\b backslash\\ slash/ formfeed\f tab\t"},
             {"boolean", boolean},
             {"null", nullable0},
             {"unicode", "∃∀‣⅕₵\001\002\003\004\005\006\007\010"}});

    std::cout << "Json output:" << std::endl;
    complex.toJson (std::cout, 0);
    std::cout << std::endl;
    complex.toJson (std::cout);
    std::cout << std::endl;

    std::cout << std::endl << "Exception details:" << std::endl;
    std::cout << "Datatype  Using as    Exception reason" << std::endl;
    exception_details ("String", str);
    exception_details ("Integer", integer);
    exception_details ("Double", doub);
    exception_details ("Boolean", boolean);
    exception_details ("Flex(s)", flex_string);
    exception_details ("Flex(i)", flex_integer);
    exception_details ("Flex(d)", flex_double);
    exception_details ("Flex(b)", flex_boolean);
    exception_details ("Flex() ", flex_empty);
    exception_details ("Flex( )", flex_whitespace);
    exception_details ("Null0", nullable0);
    exception_details ("Dict", dict);
    exception_details ("List", list);
}

template <typename T>
void json_retrieval_test (const char *message, FailureCounter &check) {
    try {
        auto data = Parsnip::parse_json (message);
        T answer = data.as<T>();
        (void) answer;
        check.succeed (message);
    } catch (const Parsnip::Exception &e) {
        check.fail (message, e.what());
    }
}

template <typename T>
void json_retrieval_exception (const char *message, FailureCounter &check) {
    try {
        auto data = Parsnip::parse_json (message);
        T answer = data.as<T>();
        (void) answer;
        check.fail (message, "Did not throw range exception");
    } catch (const Parsnip::Exception &actual) {
        check.sameException (message, Parsnip::DataRangeError(), actual);
    }
}

int check_json_value_retrieval() {
    FailureCounter check (__func__);
    json_retrieval_exception<double> ("32.44e9999", check);
    json_retrieval_exception<double> ("-32.44e9999", check);
    json_retrieval_exception<float> ("1e40", check);
    json_retrieval_exception<float> ("-1e40", check);
    json_retrieval_exception<long> ("-999999999999999999999999999999999999999999", check);
    json_retrieval_exception<long> ("999999999999999999999999999999999999999999", check);
    json_retrieval_exception<short> ("-1000000000", check);
    json_retrieval_exception<short> ("1000000000", check);

    json_retrieval_exception<unsigned short> ("-1", check);
    json_retrieval_test<unsigned short> ("65535", check);
    json_retrieval_exception<short> ("65535", check);
    json_retrieval_test<short> ("32767", check);
    return check.errorCount();
}

#ifdef PARSNIP_JSON_TRACK_POSITION
using ErrorLocation = Parsnip::DataFormatError::Location;
#else
struct ErrorLocation {
    int baka, yaro;
};
#endif
void json_parsing_exception (const char *message,
                             FailureCounter &check,
                             const ErrorLocation &expected_location) {
    try {
        auto junk = Parsnip::parse_json (message);
        check.fail ("Parsed as JSON", message);
    } catch (const Parsnip::DataFormatError &actual) {
        check.succeed (std::string (message) + ": Threw " + actual.what());
#ifdef PARSNIP_JSON_TRACK_POSITION
        ErrorLocation loc = actual.where();
        if (loc.line == 0) {
            check.skip ("Compiled without stream location counting");
        } else {
            check.equal (expected_location.line, loc.line);
            check.equal (expected_location.character, loc.character);
        }
#endif
    } catch (const Parsnip::Exception &actual) {
        check.succeed (std::string (message) + ": Threw " + actual.what());
    }
}

int check_json_encoding_exceptions() {
    FailureCounter check (__func__);
    json_parsing_exception ("This ain't JSON", check, ErrorLocation {1, 5});
    json_parsing_exception ("bakayaro", check, ErrorLocation {1, 9});
    json_parsing_exception ("{ \"this\": \"no close brace\",", check, ErrorLocation {1, 28});
    json_parsing_exception ("{ \"this\": true bare words}", check, ErrorLocation {1, 16});
    json_parsing_exception ("{ 4: \"key is not a string\"}", check, ErrorLocation {1, 3});
    json_parsing_exception ("[ 1, 2, wakka, \"invalid bare words\" ]", check, ErrorLocation {1, 14});
    json_parsing_exception ("[ 1, 2, true!, \"random punctuation\" ]", check, ErrorLocation {1, 13});
    json_parsing_exception ("[ 1, 2, \"no close brace\"", check, ErrorLocation {1, 25});
    json_parsing_exception ("[ 1, 2, \"unterminated string", check, ErrorLocation {1, 29});
    json_parsing_exception ("[ 1, 2, 2.71828.4, \"invalid number\" ]", check, ErrorLocation {1, 18});
#ifdef PARSNIP_JSON_COMMENTS
    check.subtest ("Checking commented JSON");
    json_parsing_exception ("[ 1, 2, // end of file in comment\n", check, ErrorLocation {2, 1});
    json_parsing_exception ("[ 1, 2, /* end of file in comment", check, ErrorLocation {1, 34});
#endif
    json_parsing_exception ("[\n  {\n    \"this\": \"test\"\n  },\n  false,\n  \"\\n\\nposition:\n  \"more\"\n]", check, ErrorLocation {6, 17});
#ifndef PARSNIP_JSON_HEXADECIMAL_NUMBERS
    check.subtest ("Checking hexadecimal numbers");
    json_parsing_exception ("0x1f", check, ErrorLocation {1, 1});
#endif
    return check.errorCount();
}

int parsnip_json_parser_test() {
    std::cout << "Parsed Json:" << std::endl;
    Parsnip::Data parsed = Parsnip::parse_json (
            "{ \"array\": [true, false, null], \"string\": \"eh\b\bhello, world\","
            "\"number\": 35, \"floating point\": 43.7e9,"
            "\"encoded\":\"\\u0001\\u0002\\u0003\", "
            "\"zero\": 0 }");
    std::cout << parsed.toJson (0) << std::endl;
    std::cout << parsed ["string"].asString() << std::endl;

#ifdef PARSNIP_JSON_COMMENTS
    Parsnip::Data comment_parsed = Parsnip::parse_json (
            "{ \"array\": [true, false, null], // this is a c++-stylecomment\n"
            "\"string\": \"eh\b\bhello, world\","
            "\"number\": /* c-style comment */ 35, \"floating point\": 43.7e9,"
            "\"encoded\":\"\\u0001\\u0002\\u0003\", "
            "\"zero\": 0 }");
    assert (comment_parsed == parsed);
    std::cout << comment_parsed.toJson (0) << std::endl;
#endif
#ifdef PARSNIP_JSON_HEXADECIMAL_NUMBERS
    Parsnip::Data hex_value = Parsnip::parse_json ("0x1f");
    std::cout << "Hex value parsed: " << hex_value << std::endl;
    assert (hex_value.asInteger() == 0x1f);
#endif

    int errorcount = check_json_value_retrieval();
    errorcount += check_json_encoding_exceptions();
    return errorcount;
}

/*
 *                  Parser toolchain
 */
static int create_argv_test (void) {
    FailureCounter check (__func__);
    {
        Parsnip::ArgumentVector argv ("   test of \"this\" parser, foot\"ball, \"and quot\"ing\" for it!");
        check.equal (8UL, argv.size());
        check.equal ("test", argv [0]);
        check.equal ("test of \"this\" parser, foot\"ball, \"and quot\"ing\" for it!", argv.remainder (0));
        check.equal ("of", argv [1]);
        check.equal ("of \"this\" parser, foot\"ball, \"and quot\"ing\" for it!", argv.remainder (1));
        check.equal ("this", argv [2]);
        check.equal ("\"this\" parser, foot\"ball, \"and quot\"ing\" for it!", argv.remainder (2));
        check.equal ("parser,", argv [3]);
        check.equal ("parser, foot\"ball, \"and quot\"ing\" for it!", argv.remainder (3));
        check.equal ("foot\"ball,", argv [4]);
        check.equal ("foot\"ball, \"and quot\"ing\" for it!", argv.remainder (4));
        check.equal ("and quot\"ing", argv [5]);
        check.equal ("\"and quot\"ing\" for it!", argv.remainder (5));
        check.equal ("for", argv [6]);
        check.equal ("for it!", argv.remainder (6));
        check.equal ("it!", argv [7]);
        check.equal ("it!", argv.remainder (7));
    }
    {
        check.subtest ("easy peasy");
        Parsnip::ArgumentVector argv (" \t\r\n easy peasy   ");
        check.equal (2, argv.size());
        check.equal ("easy", argv [0]);
        check.equal ("easy peasy   ", argv.remainder (0));
        check.equal ("peasy", argv [1]);
        check.equal ("peasy   ", argv.remainder (1));
    }
    {
        check.subtest ("empty command line");
        Parsnip::ArgumentVector argv ("");
        check.equal (0, argv.size());
    }
    {
        check.subtest ("First quoted");
        Parsnip::ArgumentVector argv ("\"First quoted\" next");
        check.equal (2, argv.size());
        check.equal ("First quoted", argv [0]);
        check.equal ("\"First quoted\" next", argv.remainder (0));
        check.equal ("next", argv [1]);
        check.equal ("next", argv.remainder (1));
    }

    {
        check.subtest ("Whitespace and nothing else");
        Parsnip::ArgumentVector argv (" \t\r");
        check.equal (0, argv.size());
    }

    return check.errorCount();
}

/*
 *              Parsing test functions
 */

const Parsnip::Data EmptyDictionary{Parsnip::Data::Dictionary};

static bool check_parser_exception (const Parsnip::Parser &parser,
                                    const std::string &command,
                                    FailureCounter &check,
                                    const std::exception &expect,
                                    bool check_text = true) {
    try {
        auto result = parser.evaluate (command);
        check.fail ("Exception was not thrown", command);
        std::cout << "Command ID: " << result.command_id << std::endl;
        result.parameters.dumpJson ("Parameters");
        return false;
    } catch (std::exception &actual) {
        return check.sameException (command, expect, actual);
    }
}

static bool check_parser_behavior (const Parsnip::Parser &parser,
                                   const std::string &command,
                                   FailureCounter &check,
                                   const int expected_command,
                                   const Parsnip::Data &expected_data) {
    try {
        auto result = parser.evaluate (command);
        bool one = check.equal (expected_command, result.command_id);
        bool two = check.equal (expected_data, result.parameters);
        return one && two;
    } catch (std::exception &ex) {
        return check.fail ("Exception was thrown", std::string (typeid (ex).name()) + ": " + ex.what());
    }
}

static bool check_valid_schema (FailureCounter &check,
                                const Parsnip::SchemaSet &schema,
                                const int command_id,
                                const Parsnip::Data &request_data) {
    std::string request = "[command_id " + std::to_string (command_id) + "]: " + request_data.toJson();
    try {
        schema.validate (command_id, request_data);
        return check.succeed ("Passed schema: " + request);
    } catch (Parsnip::Exception &ex) {
        check.fail ("Exception was thrown", std::string (typeid (ex).name()) + ": " + ex.what());
        std::cout << "Exception info: " << ex.what() << '\n';
        return false;
    }
}

static bool check_valid_schema (FailureCounter &check,
                                const Parsnip::SchemaSet &schema,
                                const int command_id,
                                const char *json_data) {
    return check_valid_schema (check, schema, command_id, Parsnip::parse_json (json_data));
}

static bool check_schema_validation_exception (FailureCounter &check,
                                               const Parsnip::SchemaSet &schema,
                                               const int command_id,
                                               const Parsnip::Data &request_data,
                                               const Parsnip::Exception &expect,
                                               bool same_text = true) {
    std::string request = "[command_id " + std::to_string (command_id) + "]: " + request_data.toJson();
    try {
        schema.validate (command_id, request_data);
        check.fail ("Exception was not thrown", request);
        return false;
    } catch (Parsnip::Exception &actual) {
        return check.sameException (request, expect, actual, same_text);
    }
}

static bool check_schema_validation_exception (FailureCounter &check,
                                               const Parsnip::SchemaSet &schema,
                                               const int command_id,
                                               const char *json_data,
                                               const Parsnip::Exception &expect,
                                               bool same_text = true) {
    return check_schema_validation_exception (check,
                                              schema,
                                              command_id,
                                              Parsnip::parse_json (json_data),
                                              expect,
                                              same_text);
}

inline int factorial (int v) {
    return (v <= 1 ? 1 : v * factorial (v - 1));
}

template <typename T>
void permute (std::vector<T> input, int permutation_number, std::vector<T> &output) {
    if (input.empty()) {
        return;
    }
    int index = permutation_number % input.size();
    permutation_number /= input.size();
    output.push_back (input [index]);
    input.erase (input.begin() + index);
    permute (input, permutation_number, output);
}

static void check_construction_permutations (const Parsnip::Parser::Definitions &defs, FailureCounter &check) {
    check.subtest ("Parser permutations test");
    Parsnip::Parser reference (defs);
    check.succeed ("Created reference parser");

    int permutations = factorial (defs.size());
    for (int perm_num = 1; perm_num < permutations; perm_num++) {
        Parsnip::Parser::Definitions permuted;
        permute (defs, perm_num, permuted);
        Parsnip::Parser alt_parser (permuted);
        if (reference == alt_parser) {
            check.succeed ("Permutation " + std::to_string (perm_num));
        } else {
            check.fail ("Permutation", std::to_string (perm_num));
        }
    }
}
/*
 *              Basic parsing tests
 */
static int basic_parser_test (void) {
    FailureCounter check (__func__);
    Parsnip::Parser::Definitions defs = {
            {1, ""},
            {2, "HELP [{search}]"},
            {3, "FOO BAR BAT BANG BAM"},
            {4, "USER {user} {pass}"},
    };

    Parsnip::Parser parser (defs);

    check_parser_behavior (parser, "", check, 1, EmptyDictionary);
    check_parser_behavior (parser, "HELP", check, 2, EmptyDictionary);

    Parsnip::Data expected{Parsnip::Data::Dictionary, "search", "find"};
    check_parser_behavior (parser, "HELP find", check, 2, expected);

    check_parser_behavior (parser, "FOO BAR bat bang bam", check, 3, EmptyDictionary);

    expected = Parsnip::Data::make_dictionary ({{"user", "no"}, {"pass", "baka"}});
    check_parser_behavior (parser, "USER no baka", check, 4, expected);

    /* Now feed it crap */
    check_parser_exception (parser, "BADCMD", check, Parsnip::InvalidValue ("BADCMD"));
    check_parser_exception (parser, "HELP runon stuff", check, Parsnip::RunOnCommand ("stuff"));
    check_parser_exception (parser, "FOO BAR BAKA", check, Parsnip::InvalidValue ("BAKA"));
    check_parser_exception (parser, "FOO BAR BAT", check, Parsnip::IncompleteCommand ("after BAT"));
    return check.errorCount();
}

static int regex_fillin (void) {
    FailureCounter check (__func__);
    Parsnip::Parser::Definitions defs
            = {{1, "regex sensitive {reply:/[A-Z][a-z]+/}"}, {2, "regex blind {reply:/[a-z][a-z0-9]*/i}"}};

    check_construction_permutations (defs, check);

    Parsnip::Parser parser (defs);

    check_parser_exception (parser, "regex sensitive abcdef", check, Parsnip::InvalidValue());
    check_parser_exception (parser, "regex sensitive Abc123", check, Parsnip::InvalidValue());
    check_parser_exception (parser, "regex sensitive A", check, Parsnip::InvalidValue());

    // Test that everything accepts values at the low end of the range
    Parsnip::Data expected{Parsnip::Data::Dictionary, "reply", "Abcdef"};
    check_parser_behavior (parser, "regex sensitive Abcdef", check, 1, expected);
    check_parser_behavior (parser, "regex blind Abcdef", check, 2, expected);

    expected ["reply"] = "a";
    check_parser_behavior (parser, "regex blind a", check, 2, expected);
    expected ["reply"] = "abcdef";
    check_parser_behavior (parser, "regex blind abcdef", check, 2, expected);
    expected ["reply"] = "abc123";
    check_parser_behavior (parser, "regex blind abc123", check, 2, expected);

    return check.errorCount();
}

/*
 *              Numeric fill-ins
 */

static int numeric_fillin_test (void) {
    FailureCounter check (__func__);
    Parsnip::Parser::Definitions defs = {{1, "real [{#number:2.71828182844-3.1415926535897932385}]"},
                                         {10, "decimal [{#number:10-12}]"},
                                         {16, "auto [{#number:0x010-0x020}]"}};

    check_construction_permutations (defs, check);

    Parsnip::Parser parser (defs);

    check.subtest ("special basic parsing test");
    check_parser_exception (parser, "", check, Parsnip::IncompleteCommand());

    // Test that everything accepts values at the low end of the range
    check.subtest ("Inside low end of range");
    Parsnip::Data expected{Parsnip::Data::Dictionary, "number", 2.71828182844};
    check_parser_behavior (parser, "real 2.71828182844", check, 1, expected);

    expected ["number"] = 10;
    check_parser_behavior (parser, "decimal 10", check, 10, expected);

    expected ["number"] = 16;
    check_parser_behavior (parser, "auto 16", check, 16, expected);
    check_parser_behavior (parser, "auto 020", check, 16, expected);
    check_parser_behavior (parser, "auto 0x10", check, 16, expected);

    // Test that everything accepts values at the high end of the range
    check.subtest ("Inside high end of range");
    expected ["number"] = 3.14159265;
    check_parser_behavior (parser, "real 3.14159265", check, 1, expected);

    expected ["number"] = 12;
    check_parser_behavior (parser, "decimal 12", check, 10, expected);

    expected ["number"] = 32;
    check_parser_behavior (parser, "auto 040", check, 16, expected);
    check_parser_behavior (parser, "auto 32", check, 16, expected);
    check_parser_behavior (parser, "auto 0x20", check, 16, expected);

    check.subtest ("outside minimum");
    check_parser_exception (parser, "real 2.7182", check, Parsnip::NumberOutOfRange(), false);
    check_parser_exception (parser, "decimal 0009", check, Parsnip::NumberOutOfRange(), false);
    check_parser_exception (parser, "auto 017", check, Parsnip::NumberOutOfRange(), false);
    check_parser_exception (parser, "auto 15", check, Parsnip::NumberOutOfRange(), false);
    check_parser_exception (parser, "auto 0x0f", check, Parsnip::NumberOutOfRange(), false);

    check.subtest ("outside maximum");
    check_parser_exception (parser, "real 3.141592654", check, Parsnip::NumberOutOfRange(), false);
    check_parser_exception (parser, "decimal 0009", check, Parsnip::NumberOutOfRange(), false);
    check_parser_exception (parser, "auto 041", check, Parsnip::NumberOutOfRange(), false);
    check_parser_exception (parser, "auto 33", check, Parsnip::NumberOutOfRange(), false);
    check_parser_exception (parser, "auto 0x21", check, Parsnip::NumberOutOfRange(), false);

    check.subtest ("invalid values");
    check_parser_exception (parser, "real bob", check, Parsnip::NotNumeric ("'bob'"));
    check_parser_exception (parser, "decimal billy", check, Parsnip::NotNumeric ("'billy'"));
    check_parser_exception (parser, "auto 09wrench", check, Parsnip::NotNumeric ("'09wrench'"));
    check_parser_exception (parser, "auto -43wrench", check, Parsnip::NotNumeric ("'-43wrench'"));
    check_parser_exception (parser, "auto 4.3", check, Parsnip::NotNumeric ("'4.3'"));

    check.subtest ("empty string input");
    check_parser_exception (parser, "real ''", check, Parsnip::NotNumeric ("(empty value)"));
    check_parser_exception (parser, "decimal ''", check, Parsnip::NotNumeric ("(empty value)"));
    check_parser_exception (parser, "auto ''", check, Parsnip::NotNumeric ("(empty value)"));

    check.subtest ("whitespace input");
    check_parser_exception (parser, "real '\t'", check, Parsnip::NotNumeric ("'\t'"));
    check_parser_exception (parser, "decimal '  '", check, Parsnip::NotNumeric ("'  '"));
    check_parser_exception (parser, "auto '    '", check, Parsnip::NotNumeric ("'    '"));

    return check.errorCount();
}

/* Repeat the number fill-in test with negative numbers */
static int numeric_negative_test (void) {
    FailureCounter check (__func__);
    Parsnip::Parser::Definitions def = {
            {1, "real [{#number:-5.0--1.0}]"},
            {10, "decimal [{#number:-12--10}]"},
            {16, "auto [{#number:-0x10--0x02}]"},
    };

    Parsnip::Parser parser (def);
    check.succeed ("Created parser");

    Parsnip::Data expect{Parsnip::Data::Dictionary};

    check.subtest ("inside minimum");
    expect ["number"] = -5.0;
    check_parser_behavior (parser, "real -5", check, 1, expect);

    expect ["number"] = -12;
    check_parser_behavior (parser, "decimal -12", check, 10, expect);

    expect ["number"] = -16;
    check_parser_behavior (parser, "auto -020", check, 16, expect);
    check_parser_behavior (parser, "auto -16", check, 16, expect);
    check_parser_behavior (parser, "auto -0x10", check, 16, expect);
    ;

    check.subtest ("inside maximum");
    expect ["number"] = -1.0;
    check_parser_behavior (parser, "real -1", check, 1, expect);

    expect ["number"] = -10;
    check_parser_behavior (parser, "decimal -10", check, 10, expect);

    expect ["number"] = -2;
    check_parser_behavior (parser, "auto -02", check, 16, expect);
    check_parser_behavior (parser, "auto -2", check, 16, expect);
    check_parser_behavior (parser, "auto -0x02", check, 16, expect);

    check.subtest ("outside minimum");
    check_parser_exception (parser, "real -5.000001", check, Parsnip::NumberOutOfRange(), false);
    check_parser_exception (parser, "decimal -13", check, Parsnip::NumberOutOfRange(), false);
    check_parser_exception (parser, "auto -021", check, Parsnip::NumberOutOfRange(), false);
    check_parser_exception (parser, "auto -17", check, Parsnip::NumberOutOfRange(), false);
    check_parser_exception (parser, "auto -0x11", check, Parsnip::NumberOutOfRange(), false);

    check.subtest ("outside maximum");
    check_parser_exception (parser, "real -0.999999", check, Parsnip::NumberOutOfRange(), false);
    check_parser_exception (parser, "decimal -9", check, Parsnip::NumberOutOfRange(), false);
    check_parser_exception (parser, "auto -001", check, Parsnip::NumberOutOfRange(), false);
    check_parser_exception (parser, "auto -1", check, Parsnip::NumberOutOfRange(), false);
    check_parser_exception (parser, "auto -0x01", check, Parsnip::NumberOutOfRange(), false);

    return check.errorCount();
}

static int numeric_fillin_with_text_keywords() {
    FailureCounter check (__func__);
    Parsnip::Parser::Definitions def = {
            {1, "real acceptor <keyword:foo|bar>"},
            {1, "real acceptor {#keyword:3.7-3700}"},  // Bad style, but allowed
            {2, "integer acceptor <keyword:baka|yaro>"},
            {2, "integer acceptor {#number:3-5}"},  // Bad style, but allowed
    };
    Parsnip::Parser parser (def);
    check.succeed ("Created parser");

    Parsnip::Data expect{Parsnip::Data::Dictionary, "keyword", "foo"};
    check_parser_behavior (parser, "real acceptor foo", check, 1, expect);

    expect ["keyword"] = 37.0;
    check_parser_behavior (parser, "real acceptor 37", check, 1, expect);

    expect ["keyword"] = "baka";
    check_parser_behavior (parser, "integer acceptor baka", check, 2, expect);

    expect = EmptyDictionary;
    expect ["number"] = 3;
    check_parser_behavior (parser, "integer acceptor 3", check, 2, expect);

    expect ["number"] = 5;
    check_parser_behavior (parser, "integer acceptor 5", check, 2, expect);

    check_parser_exception (parser, "integer acceptor 2", check, Parsnip::NumberOutOfRange(), false);
    check_parser_exception (parser, "integer acceptor 4.5", check, Parsnip::NotNumeric(), false);
    check_parser_exception (parser, "real acceptor baka", check, Parsnip::InvalidValue(), false);
    check_parser_exception (parser, "integer acceptor bar", check, Parsnip::InvalidValue(), false);

    return check.errorCount();
}

/*
 *              Remainder handlers
 */
static int fillin_with_remainder (void) {
    FailureCounter check (__func__);
    Parsnip::Parser::Definitions def{{2, "words [{words}] ..."}};
    Parsnip::Parser parser{def};
    check.succeed ("Created parser");

    check.subtest ("no words");
    auto result = parser.evaluate ("words");
    Parsnip::Data expect{Parsnip::Data::Dictionary, "words", Parsnip::Data{Parsnip::Data::List}};
    check.equal (expect, result.parameters);

    check.subtest ("one word");
    result = parser.evaluate ("words fish");
    expect ["words"] = Parsnip::Data{Parsnip::Data::List, "fish"};
    check.equal (expect, result.parameters);

    check.subtest ("multiple words");
    result = parser.evaluate ("words fish swim in water");
    expect ["words"] = Parsnip::Data{Parsnip::Data::List, "fish", "swim", "in", "water"};
    check.equal (expect, result.parameters);

    return check.errorCount();
}

static void validate_fillin_with_raw_remainder (const Parsnip::Parser::Definitions &def,
                                                FailureCounter &check,
                                                const std::string &foo) {
    Parsnip::Parser parser{def};
    check.succeed ("Created parser");

    Parsnip::Data expected{Parsnip::Data::Dictionary, "word", "baka"};
    check_parser_behavior (parser, "real baka", check, 2, expected);

    expected ["remainder"] = "wang";
    check_parser_behavior (parser, "real baka " + foo + " wang", check, 1, expected);

    expected ["remainder"] = "wang hobble   \"wigwag\"";
    check_parser_behavior (parser, "real baka " + foo + " wang hobble   \"wigwag\"", check, 1, expected);
}

static int fillin_with_raw_remainder (void) {
    FailureCounter check (__func__);

    check.subtest ("forward-with-separator");
    Parsnip::Parser::Definitions def = {{2, "real {word}"}, {1, "real {word} foo {remainder...}"}};
    validate_fillin_with_raw_remainder (def, check, "foo");

    check.subtest ("reverse-with-separator");
    Parsnip::Parser::Definitions redef = {{1, "real {word} foo {remainder...}"}, {2, "real {word}"}};
    validate_fillin_with_raw_remainder (redef, check, "foo");

    check.subtest ("forward-no-separator");
    Parsnip::Parser::Definitions nofoodef = {{2, "real {word}"}, {1, "real {word} {remainder...}"}};
    validate_fillin_with_raw_remainder (nofoodef, check, "");

    check.subtest ("reverse-no-separator");
    Parsnip::Parser::Definitions nofooredef = {{1, "real {word} {remainder...}"}, {2, "real {word}"}};
    validate_fillin_with_raw_remainder (nofooredef, check, "");

    return check.errorCount();
}

/*
 *                  Parser alternation
 */

static void validate_common_meals (const Parsnip::Parser &parser, FailureCounter &check) {
    check.subtest (__func__);

    Parsnip::Data expect{Parsnip::Data::Dictionary, "type", "trout"};
    check_parser_behavior (parser, "fish trout dinner", check, 3, expect);

    expect ["type"] = "halibut";
    check_parser_behavior (parser, "fish halibut lunch", check, 2, expect);

    expect ["type"] = "flounder";
    check_parser_behavior (parser, "fish flounder lunch", check, 2, expect);
    check_parser_behavior (parser, "fish flounder snack", check, 4, expect);

    expect ["type"] = "bass";
    check_parser_behavior (parser, "fish bass snack", check, 4, expect);

    expect ["type"] = "one";
    check_parser_behavior (parser, "fish one breakfast", check, 1, expect);

    check.subtest ("crossed keyword combinations");
    check_parser_exception (parser, "fish halibut dinner", check, Parsnip::InvalidValue ("dinner"));
    check_parser_exception (parser, "fish flounder dinner", check, Parsnip::InvalidValue ("dinner"));
    check_parser_exception (parser, "fish bass lunch", check, Parsnip::InvalidValue ("lunch"));
    check_parser_exception (parser, "fish flounder breakfast", check, Parsnip::InvalidValue ("breakfast"));
    check_parser_exception (parser, "fish halibut snack", check, Parsnip::InvalidValue ("snack"));
}

static int named_parallel_alternation (void) {
    // Notes:
    // - breakfast only has one alternation
    // - flounder is in both lunch and snack
    // - others are unique to meals.
    // - This is the same menu as optionals, but with alternations
    Parsnip::Parser::Definitions def = {
            {1, "fish <type:one> breakfast"},
            {2, "fish <type:flounder|halibut> lunch"},
            {3, "fish <type:trout|salmon> dinner"},
            {4, "fish <type:bass|flounder|perch> snack"},
    };

    FailureCounter check (__func__);
    Parsnip::Parser parser{def};
    check.succeed ("Created parser");

    check.subtest ("alternation not optional");
    check_parser_exception (parser, "fish breakfast", check, Parsnip::InvalidValue ("breakfast"));
    check_parser_exception (parser, "fish lunch", check, Parsnip::InvalidValue ("lunch"));
    check_parser_exception (parser, "fish dinner", check, Parsnip::InvalidValue ("dinner"));

    validate_common_meals (parser, check);
    return check.errorCount();
}

static int named_parallel_alternation_flipped (void) {
    Parsnip::Parser::Definitions def = {
            {1, "fish breakfast <type:one>"},
            {2, "fish lunch <type:flounder|halibut>"},
            {3, "fish dinner <type:trout|salmon>"},
            {4, "fish snack <type:bass|flounder|perch>"},
    };

    FailureCounter check (__func__);
    Parsnip::Parser parser{def};
    check.succeed ("Created parser");

    check.subtest ("alternation not optional");
    check_parser_exception (parser, "fish flounder", check, Parsnip::InvalidValue ("flounder"));

    check.subtest ("valid meals");
    auto result = parser.evaluate ("fish dinner trout");
    Parsnip::Data expect{Parsnip::Data::Dictionary, "type", "trout"};
    check.equal (3, result.command_id);
    check.equal (expect, result.parameters);

    result = parser.evaluate ("fish lunch halibut");
    expect ["type"] = "halibut";
    check.equal (2, result.command_id);
    check.equal (expect, result.parameters);

    result = parser.evaluate ("fish lunch flounder");
    expect ["type"] = "flounder";
    check.equal (2, result.command_id);
    check.equal (expect, result.parameters);

    result = parser.evaluate ("fish snack flounder");
    check.equal (4, result.command_id);
    check.equal (expect, result.parameters);

    result = parser.evaluate ("fish snack bass");
    expect ["type"] = "bass";
    check.equal (4, result.command_id);
    check.equal (expect, result.parameters);

    result = parser.evaluate ("fish breakfast one");
    expect ["type"] = "one";
    check.equal (1, result.command_id);
    check.equal (expect, result.parameters);

    check.subtest ("crossed alternations");
    check_parser_exception (parser, "fish dinner halibut", check, Parsnip::InvalidValue ("halibut"));
    check_parser_exception (parser, "fish dinner flounder", check, Parsnip::InvalidValue ("flounder"));
    check_parser_exception (parser, "fish lunch bass", check, Parsnip::InvalidValue ("bass"));
    check_parser_exception (parser, "fish breakfast flounder", check, Parsnip::InvalidValue ("flounder"));
    check_parser_exception (parser, "fish snack halibut", check, Parsnip::InvalidValue ("halibut"));

    return check.errorCount();
}

static int separately_named_parallel_alternation (void) {
    Parsnip::Parser::Definitions def
            = {{2, "fish <lunchtype:flounder|halibut> lunch"}, {3, "fish <dinnertype:trout|salmon> dinner"}};

    FailureCounter check (__func__);
    Parsnip::Parser parser{def};
    check.succeed ("Created parser");

    auto result = parser.evaluate ("fish trout dinner");
    Parsnip::Data expect{Parsnip::Data::Dictionary, "dinnertype", "trout"};
    check.equal (3, result.command_id);
    check.equal (expect, result.parameters);

    result = parser.evaluate ("fish halibut lunch");
    expect = Parsnip::Data{Parsnip::Data::Dictionary, "lunchtype", "halibut"};
    check.equal (2, result.command_id);
    check.equal (expect, result.parameters);

    return check.errorCount();
}

/*
 *              Parser optionals
 */
static bool named_optionals (void) {
    // Notes:
    // - breakfast only has one alternation
    // - flounder is in both lunch and snack
    // - others are unique to meals.
    // - This is the same menu as alternation, but with optionals
    Parsnip::Parser::Definitions def = {
            {1, "fish [type:one] breakfast"},
            {2, "fish [type:flounder|halibut] lunch"},
            {3, "fish [type:trout|salmon] dinner"},
            {4, "fish [type:bass|flounder|perch] snack"},
    };

    FailureCounter check (__func__);
    Parsnip::Parser parser{def};
    check.succeed ("Created parser");

    check.subtest ("optionals are optional");

    check_parser_behavior (parser, "fish breakfast", check, 1, EmptyDictionary);
    check_parser_behavior (parser, "fish lunch", check, 2, EmptyDictionary);
    check_parser_behavior (parser, "fish dinner", check, 3, EmptyDictionary);
    check_parser_behavior (parser, "fish snack", check, 4, EmptyDictionary);

    validate_common_meals (parser, check);
    return check.errorCount();
}

static int autonamed_optionals (void) {
    Parsnip::Parser::Definitions def = {{1, "fish [salmon] dinner"}};

    FailureCounter check (__func__);
    Parsnip::Parser parser{def};
    check.succeed ("Created parser");

    auto result = parser.evaluate ("fish dinner");
    check.equal (1, result.command_id);
    check.equal (EmptyDictionary, result.parameters);

    result = parser.evaluate ("fish salmon dinner");
    Parsnip::Data expected{Parsnip::Data::Dictionary, "salmon", "salmon"};
    check.equal (1, result.command_id);
    check.equal (expected, result.parameters);

    return check.errorCount();
}

static int shorter_with_optional_and_longer (void) {
    Parsnip::Parser::Definitions def = {
            {1, "fish dinner [{fishtype}]"},
            {2, "fish dinner {fishtype} for {who}"},
    };

    FailureCounter check (__func__);
    Parsnip::Parser parser{def};
    check.succeed ("Created parser");

    auto result = parser.evaluate ("fish dinner");
    check.equal (1, result.command_id);
    check.equal (EmptyDictionary, result.parameters);

    result = parser.evaluate ("fish dinner gefilte");
    Parsnip::Data expected{Parsnip::Data::Dictionary, "fishtype", "gefilte"};
    check.equal (1, result.command_id);
    check.equal (expected, result.parameters);

    result = parser.evaluate ("fish dinner gefilte for baka");
    expected ["who"] = "baka";
    check.equal (2, result.command_id);
    check.equal (expected, result.parameters);

    check_parser_exception (parser, "fish dinner gefilte for", check, Parsnip::IncompleteCommand ("after for"));

    return check.errorCount();
}

/*
 *                      Invalid parser tests
*/
static bool check_invalid_parser (const std::string &name,
                                  const Parsnip::Parser::Definitions &def,
                                  FailureCounter &check) {
    try {
        Parsnip::Parser parser (def);
        return check.fail (name, "Parser construction succeeded");
    } catch (std::runtime_error &ex) {
        return check.succeed (name + ": " + ex.what());
    }
}

static bool check_invalid_parser (const std::string &name, const char *single_line, FailureCounter &check) {
    Parsnip::Parser::Definitions def = {{1, single_line}};
    if (check_invalid_parser (name, def, check)) {
        return true;
    }
    std::cerr << "Parser statement: " << single_line << std::endl;
    return false;
}

static bool check_conflicted_parser (const std::string name, const char *one, const char *two, FailureCounter &check) {
    Parsnip::Parser::Definitions forward = {{1, one}, {2, two}};
    bool success = check_invalid_parser (name, forward, check);

    // Reverse the two definitions and try again.
    Parsnip::Parser::Definitions reverse = {{1, two}, {2, one}};
    success = check_invalid_parser (name + " (reversed)", reverse, check) && success;
    if (success) {
        return true;
    }
    std::cerr << "Parser definition: " << one << std::endl << "                   " << two << std::endl;
    return false;
}

static int invalid_parser_detection (void) {
    FailureCounter check (__func__);

    check_invalid_parser ("optional fill-in not last term", "real [{number}] foobar", check);
    check_invalid_parser ("optional before fillin", "real {number} [foo] {remainder}", check);
    check_conflicted_parser ("statement end alongside optional remainder", "real", "real [{number}] ...", check);
    check_conflicted_parser ("keyword alongside fillin", "real [{number}]", "real alongside is bad", check);
    check_conflicted_parser ("remainder alongside fillin", "real [{number}]", "real {remainder...}", check);
    check_conflicted_parser ("inconsistent ranges", "integer {#number:3-9} foo", "integer {#number:5-9} bar", check);
    check_conflicted_parser ("inconsistent ranges", "integer {#number:3-7} foo", "integer {#number:3-9} bar", check);
    check_conflicted_parser ("inconsistent ranges", "real {#number:3.5-4.7} foo", "real {#number:3.6-4.7} bar", check);
    check_conflicted_parser ("inconsistent ranges", "real {#number:3.5-4.7} foo", "real {#number:3.5-4.8} bar", check);
    check_conflicted_parser ("text fillin alongside keywords", "test <word:one|two|three>", "test {text}", check);
    check_conflicted_parser ("numeric fillin alongside keywords", "test <word:one|2|three>", "test {#number}", check);
    check_conflicted_parser ("numeric fillin alongside keywords (optional)",
                             "test [word:one|2|three]",
                             "test {#number}",
                             check);
    check_conflicted_parser ("inconsistent names",
                             "test <name:one|two|sametoken> baka",
                             "test <differename:sametoken|four|five> yaro",
                             check);
    return check.errorCount();
}

static bool aggregate_parser() {
    FailureCounter check (__func__);

    Parsnip::OptionParserRef user_options (new Parsnip::OptionParser{{
            "real {#real:0.0-10.0}",
            "decimal {#decimal:0-100}",
            "integer {#integer:0x0-0xff}",
            "password {pass}",
            "siblings {related} ...",
    }});

    Parsnip::OptionParserRef selection_predicate (new Parsnip::OptionParser{{
            "user {username}",
            "device {devicetype}",
            "where {expression...}",
            "like {match} ...",
    }});

    Parsnip::Parser parser;
    parser.addOptionParser ("options", user_options);
    parser.addOptionParser ("predicate", selection_predicate);

    parser.addStatements ({
            {1, "display record {criteria:<predicate>} ..."},
            {1, "display record <manner:long|short> {criteria:predicate} ..."},
            {2, "insert record username {user}"},
            {2, "insert record username {user} {details:options} ..."},
            {3, "update record username {user} {details:options} {criteria:predicate} ..."},
    });

    {
        Parsnip::Parser second;
        second.addOptionParser ("options", user_options);
        second.addOptionParser ("predicate", selection_predicate);
        second.addStatements ({
                {1, "display record [manner:long|short] {criteria:predicate} ..."},
                {2, "insert record username {user}"},
                {2, "insert record username {user} {details:options} ..."},
                {3, "update record username {user} {details:options} {criteria:predicate} ..."},
        });
        check.result (parser == second, "Equivalent definitions");
    }

    Parsnip::SchemaSet schema (parser);

    Parsnip::Data expected{Parsnip::Data::Dictionary};
    expected ["criteria"] = EmptyDictionary;
    expected ["criteria"]["match"] = Parsnip::Data::make_list ("abc", "def", "ghi", "jkl");
    check_parser_behavior (parser, "display record like abc def ghi jkl", check, 1, expected);
    check_valid_schema (check, schema, 1, expected);

    expected ["manner"] = "short";
    expected ["criteria"] = Parsnip::Data::make_dictionary ({
            {"username", "ringo"},
            {"devicetype", "drum"},
    });
    check_parser_behavior (parser, "display record short user ringo device drum", check, 1, expected);
    check_valid_schema (check, schema, 1, expected);

    expected ["manner"] = "long";
    expected ["criteria"]["expression"] = "band = \"beatles\" || band == \"the beatles\" || band == \"beatles, the\"";
    check_parser_behavior (
            parser,
            "display record long device drum user ringo where " + expected ["criteria"]["expression"].asString(),
            check,
            1,
            expected);
    check_valid_schema (check, schema, 1, expected);

    expected ["criteria"] = EmptyDictionary;
    expected ["criteria"]["match"] = Parsnip::Data::make_list ("ringo", "john", "george", "paul");
    check_parser_behavior (parser, "display record long like ringo john george paul", check, 1, expected);
    check_valid_schema (check, schema, 1, expected);

    expected = EmptyDictionary;
    expected ["user"] = "jimbob";
    expected ["details"] = Parsnip::Data::make_dictionary ({
            {"pass", "waltons"},
            {"integer", 64},
            {"real", 9.7},
            {"related", Parsnip::Data::make_list ("marysue", "maryellen", "esther", "jason")},
    });
    check_parser_behavior (parser,
                           "insert record username jimbob integer 0x40 password waltons real '9.7' "
                           " siblings marysue maryellen esther jason",
                           check,
                           2,
                           expected);
    check_valid_schema (check, schema, 2, expected);

    expected = EmptyDictionary;
    expected ["user"] = "jimbob";
    expected ["details"] = EmptyDictionary;
    expected ["details"]["decimal"] = 43;
    expected ["criteria"] = EmptyDictionary;
    expected ["criteria"]["devicetype"] = "fence";
    expected ["criteria"]["match"] = Parsnip::Data::make_list ("wooden", "split-rail");
    check_parser_behavior (parser,
                           "update record username jimbob decimal 43 device fence like wooden split-rail",
                           check,
                           3,
                           expected);
    check_valid_schema (check, schema, 3, expected);

    check_parser_exception (parser,
                            "display record user ringo device drum user john",
                            check,
                            Parsnip::DuplicateOption ("john"));

    return check.errorCount();
}

/*
 *                  Schema Tests
 */

static bool schema_member_presence_checks() {
    FailureCounter check (__func__);

    // clang-format off
    Parsnip::Parser parser ({
        {1, "string <string:one|of|these|keywords>"},
        {2, "number {#real:0.0-3.5}"},
        {3, "integer {#integer:4-17}"},
        {4, "optional integer [{#optint:7-9}]"},
        {5, "list integer [{#listofint:20-25}] ..."},
        {6, "list at least one integer {#listnotempty:30-32} ..."},
        {7, "always {here} and string {string}"},
        {7, "always {here} and number {#real:0.0-3.5}"},
        {7, "always {here} and integer {#integer:4-17}"},
        {7, "always {here} and always {dependent} [{#optint:7-9}]"}
    });
    // clang-format on

    Parsnip::SchemaSet schema (parser);

    Parsnip::Data data{Parsnip::Data::Dictionary};
    check_schema_validation_exception (check, schema, 1, data, Parsnip::NoSuchKey ("string"));
    check_valid_schema (check, schema, 4, data);
    check_schema_validation_exception (check, schema, 3, data, Parsnip::NoSuchKey ("integer"));
    check_schema_validation_exception (check, schema, 6, data, Parsnip::NoSuchKey ("listnotempty"));

    data ["string"] = "one";
    check_valid_schema (check, schema, 1, data);
    check_schema_validation_exception (check, schema, 2, data, Parsnip::NoSuchKey ("real"));
    check_schema_validation_exception (check, schema, 3, data, Parsnip::NoSuchKey ("integer"));
    check_schema_validation_exception (check, schema, 6, data, Parsnip::NoSuchKey ("listnotempty"));

    data = EmptyDictionary;
    data ["real"] = -1;
    check_schema_validation_exception (check,
                                       schema,
                                       2,
                                       data,
                                       Parsnip::NumberOutOfRange ("-1.000000 (range: 0.000000-3.500000)"));
    data ["real"] = 3.6;
    check_schema_validation_exception (check,
                                       schema,
                                       2,
                                       data,
                                       Parsnip::NumberOutOfRange ("3.600000 (range: 0.000000-3.500000)"));
    data ["real"] = 1;
    check_schema_validation_exception (check, schema, 1, data, Parsnip::NoSuchKey ("string"));
    check_valid_schema (check, schema, 2, data);
    check_schema_validation_exception (check, schema, 3, data, Parsnip::NoSuchKey ("integer"));
    check_schema_validation_exception (check, schema, 6, data, Parsnip::NoSuchKey ("listnotempty"));

    data ["integer"] = 4;
    check_schema_validation_exception (check, schema, 1, data, Parsnip::NoSuchKey ("string"));
    check_schema_validation_exception (check, schema, 2, data, Parsnip::InvalidKey ("integer"));
    check_schema_validation_exception (check, schema, 3, data, Parsnip::InvalidKey ("real"));

    data.remove ("real");
    check_valid_schema (check, schema, 3, data);

    data = Parsnip::Data::make_dictionary ({{"optint", 8}});
    check_valid_schema (check, schema, 4, data);
    check_schema_validation_exception (check, schema, 7, data, Parsnip::NoSuchKey ("?"), false);

    data = Parsnip::Data::make_dictionary ({{"dependent", "foo"}});
    check_schema_validation_exception (check, schema, 7, data, Parsnip::NoSuchKey ("here"), false);

    data ["here"] = "yes";
    check_valid_schema (check, schema, 7, data);

    data ["optint"] = 8;
    check_valid_schema (check, schema, 7, data);

    data.remove ("dependent");
    check_schema_validation_exception (check, schema, 7, data, Parsnip::NoSuchKey ("dependent (expected by optint)"));

    data ["dependent"] = "foo";
    data ["baka"] = "hi mom";
    check_schema_validation_exception (check, schema, 7, data, Parsnip::InvalidKey ("baka"));

    return check.errorCount();
}

static bool schema_data_value_checks() {
    FailureCounter check (__func__);

    // clang-format off
    Parsnip::Parser parser ({
        {1, "string {string}"},
        {2, "number {#real:0.0-3.5}"},
        {3, "integer {#integer:4-17}"},
        {4, "optional integer [{#optint:-70--9}]"},
        {5, "list integer [{#listofint:20-25}] ..."},
        {6, "list at least one integer {#listnotempty:30-32} ..."},
        {7, "keyword <keyword:one|of|these|words>"},
        {8, "optional [keyword:one|of|these|words]"}
    });
    // clang-format on

    Parsnip::SchemaSet schema (parser);

    Parsnip::Data data{Parsnip::Data::Dictionary};

    //        {1, "string {string}"},
    check_valid_schema (check, schema, 1, "{'string': '! ~]abc123xyzABCDEF|?\\\'\"'}");
    check_schema_validation_exception (check,
                                       schema,
                                       1,
                                       "{'string': '\\f'}",
                                       Parsnip::InvalidValue ("Control characters in string"));
    check_schema_validation_exception (check,
                                       schema,
                                       1,
                                       "{'string': '\\u0001'}",
                                       Parsnip::InvalidValue ("Control characters in string"));
    check_schema_validation_exception (check,
                                       schema,
                                       1,
                                       "{'string': '\\u001f'}",
                                       Parsnip::InvalidValue ("Control characters in string"));
    check_schema_validation_exception (check,
                                       schema,
                                       1,
                                       "{'string': '\\n'}",
                                       Parsnip::InvalidValue ("Control characters in string"));
    check_schema_validation_exception (check,
                                       schema,
                                       1,
                                       "{'string': '\\b'}",
                                       Parsnip::InvalidValue ("Control characters in string"));
    check_schema_validation_exception (check,
                                       schema,
                                       1,
                                       "{'string': '\\r'}",
                                       Parsnip::InvalidValue ("Control characters in string"));
    check_schema_validation_exception (
            check,
            schema,
            1,
            "{'string': 42}",
            Parsnip::IncorrectDataType (Parsnip::Data::Type::String, Parsnip::Data::Type::Integer));

    //        {2, "number {#real:0.0-3.5}"},
    check_valid_schema (check, schema, 2, "{'real': 0.0}");
    check_valid_schema (check, schema, 2, "{'real': 3.5}");
    check_schema_validation_exception (check, schema, 2, "{'real': -0.0000001}", Parsnip::NumberOutOfRange (""), false);
    check_schema_validation_exception (check, schema, 2, "{'real': 3.5000001}", Parsnip::NumberOutOfRange (""), false);
    check_schema_validation_exception (
            check,
            schema,
            2,
            "{'real': 'fred'}",
            Parsnip::IncorrectDataType (Parsnip::Data::Type::Real, Parsnip::Data::Type::String));

    //        {3, "integer {#integer:4-17}"},
    check_valid_schema (check, schema, 3, "{'integer': 4}");
    check_valid_schema (check, schema, 3, "{'integer': 17}");
    check_schema_validation_exception (check,
                                       schema,
                                       3,
                                       "{'integer': 3}",
                                       Parsnip::NumberOutOfRange ("3 (range: 4-17)"));
    check_schema_validation_exception (check,
                                       schema,
                                       3,
                                       "{'integer': 18}",
                                       Parsnip::NumberOutOfRange ("18 (range: 4-17)"));
    check_schema_validation_exception (
            check,
            schema,
            3,
            "{'integer': 42.3}",
            Parsnip::IncorrectDataType (Parsnip::Data::Type::Integer, Parsnip::Data::Type::Real));
    check_schema_validation_exception (
            check,
            schema,
            3,
            "{'integer': [30, 31, 32]}",
            Parsnip::IncorrectDataType (Parsnip::Data::Type::Integer, Parsnip::Data::Type::List));

    //        {4, "optional integer [{#optint:-70--9}]"},
    check_valid_schema (check, schema, 4, "{'optint': -70}");
    check_valid_schema (check, schema, 4, "{'optint': -9}");
    check_schema_validation_exception (check,
                                       schema,
                                       4,
                                       "{'optint': -71}",
                                       Parsnip::NumberOutOfRange ("-71 (range: -70--9)"));
    check_schema_validation_exception (check,
                                       schema,
                                       4,
                                       "{'optint': -8}",
                                       Parsnip::NumberOutOfRange ("-8 (range: -70--9)"));
    check_schema_validation_exception (
            check,
            schema,
            4,
            "{'optint': [43]}",
            Parsnip::IncorrectDataType (Parsnip::Data::Type::Integer, Parsnip::Data::Type::List));

    //        {5, "list integer [{#listofint:20-25}] ..."},
    check_valid_schema (check, schema, 5, "{'listofint': [25]}");
    check_valid_schema (check, schema, 5, "{'listofint': [20, 22, 24, 25]}");
    check_valid_schema (check, schema, 5, "{'listofint': []}");
    check_schema_validation_exception (check,
                                       schema,
                                       5,
                                       "{'listofint': [22, 19, 37, 43]}",
                                       Parsnip::NumberOutOfRange ("19 (range: 20-25)"));
    check_schema_validation_exception (
            check,
            schema,
            5,
            "{'listofint': '43'}",
            Parsnip::IncorrectDataType (Parsnip::Data::Type::List, Parsnip::Data::Type::String));

    //        {6, "list at least one integer {#listnotempty:30-32} ..."},
    check_valid_schema (check, schema, 6, "{'listnotempty': [30]}");
    check_valid_schema (check, schema, 6, "{'listnotempty': [30, 31, 32]}");
    check_schema_validation_exception (check, schema, 6, "{'listnotempty': []}", Parsnip::IncorrectListSize(), false);
    check_schema_validation_exception (check,
                                       schema,
                                       6,
                                       "{'listnotempty': [30, 31, 33]}",
                                       Parsnip::NumberOutOfRange ("33 (range: 30-32)"));
    check_schema_validation_exception (
            check,
            schema,
            6,
            "{'listnotempty': {'value':43}}",
            Parsnip::IncorrectDataType (Parsnip::Data::Type::List, Parsnip::Data::Type::Dictionary));

    //        {7, "keyword <keyword:one|of|these|words>"},
    check_valid_schema (check, schema, 7, "{'keyword': 'one'}");
    check_valid_schema (check, schema, 7, "{'keyword': 'words'}");
    check_schema_validation_exception (check, schema, 7, "{'keyword': 'smeg'}", Parsnip::InvalidValue ("smeg"));
    check_schema_validation_exception (check, schema, 7, "{}", Parsnip::NoSuchKey ("keyword"));

    //        {8, "optional [keyword:one|of|these|words]")
    check_valid_schema (check, schema, 8, "{'keyword': 'one'}");
    check_valid_schema (check, schema, 8, "{'keyword': 'words'}");
    check_valid_schema (check, schema, 8, EmptyDictionary);
    check_schema_validation_exception (
            check,
            schema,
            8,
            "{'keyword': true}",
            Parsnip::IncorrectDataType (Parsnip::Data::Type::String, Parsnip::Data::Type::Boolean));

    return check.errorCount();
}

static bool predefined_schema_tests() {
    FailureCounter check (__func__);

    Parsnip::Data test_list;
    const char *test_filename = "schema_tests.json";

    try {
        std::ifstream in (test_filename);
        if (!in.good()) {
            check.skip (std::string (test_filename) + ": file not found.");
            return (0);
        }
        test_list = Parsnip::parse_json (in);
    } catch (std::exception &ex) {
        check.fail ("Could not load test data", ex.what());
        return check.errorCount();
    }
    std::clog << "Test data loaded\n";
    for (const Parsnip::Data &test_set : test_list) {
        check.subtest (test_set.getOr ("name", "Unnamed Test Group"));
        Parsnip::Schema schema{};
        try {
            schema = Parsnip::Schema{test_set ["schema"]};
        } catch (std::regex_error &ex) {
            check.warning ("The underlying implementation of regex is borked.");
            std::cerr << ex.what() << std::endl;
            continue;
        }
        try {
            for (const Parsnip::Data &test : test_set ["tests"]) {
                std::string result = "UNKNOWN";
                std::string details;
                try {
                    schema.validate (test ["data"]);
                    result = "valid";
                } catch (Parsnip::NoSuchKey &ex) {
                    result = "NoSuchKey";
                    details = ex.what();
                } catch (Parsnip::NumberOutOfRange &ex) {
                    result = "NumberOutOfRange";
                    details = ex.what();
                } catch (Parsnip::IncorrectDataType &ex) {
                    result = "IncorrectDataType";
                    details = ex.what();
                } catch (Parsnip::InvalidValue &ex) {
                    result = "InvalidValue";
                    details = ex.what();
                } catch (Parsnip::InvalidKey &ex) {
                    result = "InvalidKey";
                    details = ex.what();
                } catch (Parsnip::InvalidSchema &ex) {
                    result = "InvalidSchema";
                    details = ex.what();
                } catch (std::exception &ex) {
                    result = "Exception, NOS: ";
                    details = ex.what();
                }
                const std::string &expected = test ["expected"].asString();
                check.result (result == expected,
                              test.getOr ("name", "Unnamed test"),
                              "Expect: " + expected + ", actual=" + result);
                if (result != expected) {
                    std::cerr << "Exception details: " << details << '\n';
                    schema.dump ("Schema");
                }
            }
        } catch (Parsnip::Exception &ex) {
            check.fail ("Test definition is broken", ex.what());
        }
    }
    return check.errorCount();
}

/*
 *                  Test Drivers
 */

// Make a whole bunch of allocations to make any leaks obvious to Valgrind
void leak_test (int leak_iterations) {
    Parsnip::Data dict{Parsnip::Data::Dictionary, "Initial", "data"};
    Parsnip::Data list{Parsnip::Data::List, 1, 3, 5, 7, 9};

    std::cerr << "Leak check: Parsnip::Data dictionary and list copy" << std::endl;
    for (int i = leak_iterations; i; i--) {
        if ((i & 0xfff) == 0) {
            std::cerr << i << " \r";
        }
        Parsnip::Data dictcopy (dict);
        Parsnip::Data listcopy (list);
        dictcopy = list;
        listcopy = dict;
    }

    std::cerr << "Leak check: Parsnip::Data dictionary insertion" << std::endl;
    for (int i = leak_iterations; i; i--) {
        if ((i & 0x3ff) == 0) {
            std::cerr << i << " \r";
        }
        dict ["item_" + std::to_string (i)] = "Hello for the " + std::to_string (i) + "th time";
        list.push_back (i);
    }

    std::cerr << "Leak check: Parser creation and evaluation" << std::endl;
    Parsnip::Parser::Definitions defs{{1, "This is a parser definition"},
                                      {2, "HELP [{search}]"},
                                      {3, "FOO BAR BAT BANG BAM"},
                                      {4, "USER {user} {pass}"}};
    for (int i = leak_iterations / 10; i; i--) {
        if ((i & 0xfff) == 0) {
            std::cerr << i << " \r";
        }
        Parsnip::Parser parser (defs);
        parser.evaluate ("user test value");
    }

    std::cerr << "Done.  Check valgrind or other memory check logs for results." << std::endl;
}

#define do_test(test_name)                                       \
    if (requested_test == nullptr || strcasecmp (requested_test, (#test_name)) == 0) \
        test_name();

void parsnip_line_parser_test (const char *requested_test) {
    // Basic and {fill-in} tests.
    do_test (create_argv_test);
    do_test (basic_parser_test);
    do_test (numeric_fillin_test);
    do_test (numeric_negative_test);
    do_test (numeric_fillin_with_text_keywords);
    do_test (regex_fillin);

    // Test remainder fields
    do_test (fillin_with_remainder);
    do_test (fillin_with_raw_remainder);

    // Test <field|alternation>
    do_test (named_parallel_alternation);
    do_test (named_parallel_alternation_flipped);
    do_test (separately_named_parallel_alternation);

    // Test [optional] fields.
    do_test (shorter_with_optional_and_longer);
    do_test (named_optionals);
    do_test (autonamed_optionals);

    // These tests test the parser builder's error handling.
    do_test (invalid_parser_detection);

    // Test aggregate parsers
    do_test (aggregate_parser);
}

void parsnip_schema_test (const char *requested_test) {
    do_test (schema_member_presence_checks);
    do_test (schema_data_value_checks);
    do_test (predefined_schema_tests);
}

int main (int argc, char **argv) {
    const char *name = *argv;
    int leak_iterations = 0;
    int flag;
    const char *requested_test = nullptr;

    while ((flag = getopt (argc, argv, "chl:t:")) > 0) {
        switch (flag) {
            case 'h':
            case '?':
                std::cerr << name << " [-h] [-c] [-l leak-iterations] [-t test_name]" << std::endl;
                exit (EXIT_SUCCESS);
            case 'c':
                FailureCounter::initialize_colors();
                break;
            case 'l':
                leak_iterations = atol (optarg);
                break;
            case 't':
                requested_test = optarg;
                break;
            default:
                std::cerr << "Unimplemented option '" << (char) flag << '\'' << std::endl;
                exit (EXIT_FAILURE);
        }
    }

    do_test (parsnip_data_test);
    do_test (parsnip_json_parser_test);
    parsnip_line_parser_test (requested_test);
    parsnip_schema_test (requested_test);
    if (leak_iterations) {
        leak_test (leak_iterations);
    }
    FailureCounter::summary();
    return (FailureCounter::grandTotalErrors() == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
