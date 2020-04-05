#include <gtest/gtest.h>

#include "json.hpp"

TEST(Parser, Integer) {
    json::Parser parser;
    const auto value = parser.ParseFromString("123");
    EXPECT_TRUE(value.valid());
    EXPECT_EQ(json::Value::NUMBER, value.type());
    EXPECT_EQ(123.0, parser.AsNumber(value));
}

TEST(Parser, Double) {
    json::Parser parser;
    const auto value = parser.ParseFromString("123.567");
    EXPECT_TRUE(value.valid());
    EXPECT_EQ(json::Value::NUMBER, value.type());
    EXPECT_EQ(123.567, parser.AsNumber(value));
}

TEST(Parser, True) {
    json::Parser parser;
    const auto value = parser.ParseFromString("true");
    EXPECT_TRUE(value.valid());
    EXPECT_EQ(json::Value::BOOL, value.type());
    EXPECT_TRUE(parser.AsBool(value));
}

TEST(Parser, False) {
    json::Parser parser;
    const auto value = parser.ParseFromString("false");
    EXPECT_TRUE(value.valid());
    EXPECT_EQ(json::Value::BOOL, value.type());
    EXPECT_FALSE(parser.AsBool(value));
}

TEST(Parser, SimpleArray) {
    json::Parser parser;
    const auto value = parser.ParseFromString("[1, 2, 3, 4, 5, 6]");
    EXPECT_TRUE(value.valid());
    EXPECT_EQ(json::Value::ARRAY, value.type());
    EXPECT_EQ(6, parser.ArraySize(value));
    EXPECT_EQ(1.0, parser.AsNumber(parser.ArrayElem(value, 0)));
    EXPECT_EQ(2.0, parser.AsNumber(parser.ArrayElem(value, 1)));
    EXPECT_EQ(3.0, parser.AsNumber(parser.ArrayElem(value, 2)));
    EXPECT_EQ(4.0, parser.AsNumber(parser.ArrayElem(value, 3)));
    EXPECT_EQ(5.0, parser.AsNumber(parser.ArrayElem(value, 4)));
    EXPECT_EQ(6.0, parser.AsNumber(parser.ArrayElem(value, 5)));
}

TEST(Parser, SimpleObject) {
    json::Parser parser;
    const auto value = parser.ParseFromString(R"(
{
    "n1" : "v1",
    "n2": "v2",
    "n3" :"v3"
}
)");
    EXPECT_TRUE(value.valid());
    EXPECT_EQ(json::Value::OBJECT, value.type());
    EXPECT_EQ(3, parser.ObjectSize(value));
    EXPECT_EQ("v1", parser.AsString(parser.ObjectProp(value, "n1")));
    EXPECT_EQ("v2", parser.AsString(parser.ObjectProp(value, "n2")));
    EXPECT_EQ("v3", parser.AsString(parser.ObjectProp(value, "n3")));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
