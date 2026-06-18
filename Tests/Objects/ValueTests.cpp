//
//  ValueTests.cpp
//  Rayne Unit Tests
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "../Shared/Bootstrap.h"

class ValueTests : public KernelFixture
{
};

TEST_F(ValueTests, HalfVectorTypeTests)
{
	RN::Value *halfVector2Value = RN::Value::WithHalfVector2(RN::HalfVector2(1.0f, 2.0f));
	RN::Value *halfVector3Value = RN::Value::WithHalfVector3(RN::HalfVector3(1.0f, 2.0f, 3.0f));
	RN::Value *halfVector4Value = RN::Value::WithHalfVector4(RN::HalfVector4(1.0f, 2.0f, 3.0f, 4.0f));

	ASSERT_EQ(RN::TypeTranslator<RN::HalfVector2>::value, halfVector2Value->GetValueType());
	ASSERT_EQ(RN::TypeTranslator<RN::HalfVector3>::value, halfVector3Value->GetValueType());
	ASSERT_EQ(RN::TypeTranslator<RN::HalfVector4>::value, halfVector4Value->GetValueType());

	ASSERT_TRUE(halfVector2Value->CanConvertToType<RN::HalfVector2>());
	ASSERT_TRUE(halfVector3Value->CanConvertToType<RN::HalfVector3>());
	ASSERT_TRUE(halfVector4Value->CanConvertToType<RN::HalfVector4>());
}

TEST_F(ValueTests, HalfVectorConversionTests)
{
	RN::HalfVector2 halfVector2 = RN::Value::WithHalfVector2(RN::HalfVector2(1.0f, 2.0f))->GetValue<RN::HalfVector2>();
	RN::HalfVector3 halfVector3 = RN::Value::WithHalfVector3(RN::HalfVector3(1.0f, 2.0f, 3.0f))->GetValue<RN::HalfVector3>();
	RN::HalfVector4 halfVector4 = RN::Value::WithHalfVector4(RN::HalfVector4(1.0f, 2.0f, 3.0f, 4.0f))->GetValue<RN::HalfVector4>();

	RN::Vector2 vector2 = halfVector2.GetVector2();
	RN::Vector3 vector3 = halfVector3.GetVector3();
	RN::Vector4 vector4 = halfVector4.GetVector4();

	ASSERT_EQ(1.0f, vector2.x);
	ASSERT_EQ(2.0f, vector2.y);

	ASSERT_EQ(1.0f, vector3.x);
	ASSERT_EQ(2.0f, vector3.y);
	ASSERT_EQ(3.0f, vector3.z);

	ASSERT_EQ(1.0f, vector4.x);
	ASSERT_EQ(2.0f, vector4.y);
	ASSERT_EQ(3.0f, vector4.z);
	ASSERT_EQ(4.0f, vector4.w);
}
