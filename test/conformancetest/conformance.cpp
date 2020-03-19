
#if defined(__clang__) || defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2))
#if defined(__clang__) || (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
#pragma GCC diagnostic push
#endif
#pragma GCC diagnostic ignored "-Weffc++"
#endif

#include "gtest/gtest.h"

#if defined(__clang__) || defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
#pragma GCC diagnostic pop
#endif

#include "rapidjson/schema.h"
#include <ctime>
#include <string>
#include <vector>

#define ARRAY_SIZE(a) sizeof(a) / sizeof(a[0])

using namespace rapidjson;

RAPIDJSON_DIAG_PUSH
#if defined(__GNUC__) && __GNUC__ >= 7
RAPIDJSON_DIAG_OFF(format-overflow)
#endif

template <typename Allocator>
static char* ReadFile(const char* filename, Allocator& allocator) {
    const char *paths[] = {
        "",
        "bin/",
        "../bin/",
        "../../bin/",
        "../../../bin/"
    };
    char buffer[1024];
    FILE *fp = 0;
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        sprintf(buffer, "%s%s", paths[i], filename);
        fp = fopen(buffer, "rb");
        if (fp)
            break;
    }

    if (!fp)
        return 0;

    fseek(fp, 0, SEEK_END);
    size_t length = static_cast<size_t>(ftell(fp));
    fseek(fp, 0, SEEK_SET);
    char* json = reinterpret_cast<char*>(allocator.Malloc(length + 1));
    size_t readLength = fread(json, 1, length, fp);
    json[readLength] = '\0';
    fclose(fp);
    return json;
}

RAPIDJSON_DIAG_POP

class RemoteProvider : public IRemoteSchemaDocumentProvider
{
public:
    RemoteProvider() = default;

    const SchemaDocument* GetRemoteDocument(const Ch *uri, SizeType /*length*/) override {
        const char removal[] = "http://localhost:1234/";
        char filename[FILENAME_MAX];
        const char* localFile = strstr(uri, removal);

        if (localFile != nullptr) {
            char referenceFile[FILENAME_MAX];
            strncpy(referenceFile, &uri[sizeof(removal) - 1], FILENAME_MAX);
            sprintf(filename, "jsonschema/remotes/%s", referenceFile);

            char* internalRef = strstr(filename, "#");
            if (internalRef != nullptr) {
                *internalRef = '\0';
            }

            char jsonBuffer[65536];
            MemoryPoolAllocator<> jsonAllocator(jsonBuffer, sizeof(jsonBuffer));
            char* json = ReadFile(filename, jsonAllocator);
            if (!json) {
                printf("json test suite file %s not found", filename);
                return nullptr;
            }

            Document d;
            d.Parse(json);
            if (d.HasParseError()) {
                printf("json test suite file %s has parse error", filename);
                return nullptr;
            }
            return new SchemaDocument(d);
        } else {
            printf("external file %s not supported", uri);
            return nullptr;
        }
    }
};

class Conformance : public ::testing::TestWithParam<const char*> {
public:
    Conformance() :
      draftVersion{'\0'}
    {}

    void SetUp() override {
        char jsonBuffer[65536];
        MemoryPoolAllocator<> jsonAllocator(jsonBuffer, sizeof(jsonBuffer));

        char filename[FILENAME_MAX];
        sprintf(filename, "jsonschema/tests/%s/%s", draftVersion, GetParam());
        char* json = ReadFile(filename, jsonAllocator);
        if (!json) {
            printf("json test suite file %s not found", filename);
            return;
        }

        Document d;
        d.Parse(json);
        if (d.HasParseError()) {
            printf("json test suite file %s has parse error", filename);
            return;
        }

        for (Value::ConstValueIterator schemaItr = d.Begin(); schemaItr != d.End(); ++schemaItr) {
            std::string schemaDescription = (*schemaItr)["description"].GetString();
            if (IsExcludeTestSuite(schemaDescription))
                continue;

            TestSuite* ts = new TestSuite;
            RemoteProvider provider;
            ts->schema = new SchemaDocument((*schemaItr)["schema"], nullptr, 0, &provider);

            ts->description = new char[(*schemaItr)["description"].GetStringLength() + 1];
            strncpy(ts->description, (*schemaItr)["description"].GetString(), (*schemaItr)["description"].GetStringLength());
            ts->description[(*schemaItr)["description"].GetStringLength()] = '\0';

            const Value& tests = (*schemaItr)["tests"];
            for (Value::ConstValueIterator testItr = tests.Begin(); testItr != tests.End(); ++testItr) {
                if (IsExcludeTest(schemaDescription + ", " + (*testItr)["description"].GetString()))
                    continue;
                TestDocument* testDoc = new TestDocument();
                testDoc->document = new Document;
                testDoc->document->CopyFrom((*testItr)["data"], testDoc->document->GetAllocator());

                testDoc->description = new char[(*testItr)["description"].GetStringLength() + 1];
                strncpy(testDoc->description, (*testItr)["description"].GetString(), (*testItr)["description"].GetStringLength());
                testDoc->description[(*testItr)["description"].GetStringLength()] = '\0';

                testDoc->valid = (*testItr)["valid"].GetBool();

                ts->tests.push_back(testDoc);
            }
            testSuites.push_back(ts);
        }

    }

    void TearDown() override {
        for (TestSuiteList::const_iterator itr = testSuites.begin(); itr != testSuites.end(); ++itr)
            delete *itr;
        testSuites.clear();
    }
protected:
    char draftVersion[20];

private:
    // Using the same exclusion in https://github.com/json-schema/JSON-Schema-Test-Suite
    static bool IsExcludeTestSuite(const std::string& description) {
        const char* excludeTestSuites[] = {
            //lost failing these tests
//            "remote ref",
            "remote ref, containing refs itself",
//            "fragment within remote ref",
//            "ref within remote ref",
            "change resolution scope",
            // these below were added to get jsck in the benchmarks)
            "uniqueItems validation",
            "valid definition",
            "invalid definition"
        };

        for (size_t i = 0; i < ARRAY_SIZE(excludeTestSuites); i++)
            if (excludeTestSuites[i] == description)
                return true;
        return false;
    }

    // Using the same exclusion in https://github.com/json-schema/JSON-Schema-Test-Suite
    static bool IsExcludeTest(const std::string& description) {
        const char* excludeTests[] = {
            //lots of validators fail these
            "invalid definition, invalid definition schema",
            "maxLength validation, two supplementary Unicode code points is long enough",
            "minLength validation, one supplementary Unicode code point is not long enough",
            //this is to get tv4 in the benchmarks
            "heterogeneous enum validation, something else is invalid"
        };

        for (size_t i = 0; i < ARRAY_SIZE(excludeTests); i++)
            if (excludeTests[i] == description)
                return true;
        return false;
    }

    Conformance(const Conformance&);
    Conformance& operator=(const Conformance&);

protected:

    struct TestDocument {
        TestDocument() : document(), description(), valid(false) {}
        ~TestDocument() {
            delete document;
            delete description;
        }
        Document* document;
        char* description;
        bool valid;
    };
    typedef std::vector<TestDocument*> DocumentList;

    struct TestSuite {
        TestSuite() : schema(), tests(), description() {}
        ~TestSuite() {
            delete schema;
            delete description;
            for (DocumentList::iterator itr = tests.begin(); itr != tests.end(); ++itr)
                delete *itr;
        }
        SchemaDocument* schema;
        DocumentList tests;
        char* description;
    };

    typedef std::vector<TestSuite* > TestSuiteList;
    TestSuiteList testSuites;
};

class Draft04 : public Conformance {
public:
    Draft04() : Conformance() {
        strncpy(draftVersion, "draft4", 7);
        draftVersion[6] = '\0';
    }
};
const char* draft04files[] = {
        "additionalItems.json",
        "additionalProperties.json",
        "allOf.json",
        "anyOf.json",
        "default.json",
        "definitions.json",
        "dependencies.json",
        "enum.json",
        "items.json",
        "maximum.json",
        "maxItems.json",
        "maxLength.json",
        "maxProperties.json",
        "minimum.json",
        "minItems.json",
        "minLength.json",
        "minProperties.json",
        "multipleOf.json",
        "not.json",
        "oneOf.json",
        "pattern.json",
        "patternProperties.json",
        "properties.json",
        "ref.json",
        "refRemote.json",
        "required.json",
        "type.json",
        "uniqueItems.json"
};

INSTANTIATE_TEST_CASE_P(Draft04, Draft04, ::testing::ValuesIn(draft04files));

TEST_P(Draft04, TestSuite) {
    char validatorBuffer[65536];
    MemoryPoolAllocator<> validatorAllocator(validatorBuffer, sizeof(validatorBuffer));

    for (TestSuiteList::const_iterator itr = testSuites.begin(); itr != testSuites.end(); ++itr) {
        const TestSuite& ts = **itr;
        GenericSchemaValidator<SchemaDocument, BaseReaderHandler<UTF8<> >, MemoryPoolAllocator<> >  validator(*ts.schema, &validatorAllocator);
        for (DocumentList::const_iterator testItr = ts.tests.begin(); testItr != ts.tests.end(); ++testItr) {
            validator.Reset();
            if ((*testItr)->valid) {
                EXPECT_TRUE((*testItr)->document->Accept(validator))
                                    << ts.description << " :: " << (*testItr)->description << "\n";
            } else {
                EXPECT_FALSE((*testItr)->document->Accept(validator))
                                    << ts.description << " :: " << (*testItr)->description << "\n";
            }
        }
        validatorAllocator.Clear();
    }
}