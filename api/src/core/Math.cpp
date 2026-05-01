#include "../../include/core/Math.hpp"



extern "C"
{
    #include "../../../core/math/Vector2.h"
    #include "../../../core/math/Vector3.h"
    #include "../../../core/math/Quaternion.h"
    #include "../../../core/math/Matrix4.h"
}



namespace Prism
{
    // ==========================================
    // Vector2 Implementation
    // ==========================================

    void Vector2::Normalize() {
        ::Vector2 raw = ::Vector2Normalize({x, y});
        x = raw.x; y = raw.y;
    }
    Vector2 Vector2::Normalized() const {
        ::Vector2 raw = ::Vector2Normalize({x, y});
        return Vector2(raw.x, raw.y);
    }
    float Vector2::Dot(const Vector2& a, const Vector2& b) {
        return ::Vector2Dot({a.x, a.y}, {b.x, b.y});
    }
    Vector2 Vector2::Lerp(const Vector2& start, const Vector2& end, float t) {
        ::Vector2 raw = ::Vector2Lerp({start.x, start.y}, {end.x, end.y}, t);
        return Vector2(raw.x, raw.y);
    }



    // ==========================================
    // Vector3 Implementation
    // ==========================================

    void Vector3::Normalize() {
        ::Vector3 raw = ::Vector3Normalize({x, y, z});
        x = raw.x; y = raw.y; z = raw.z;
    }
    Vector3 Vector3::Normalized() const {
        ::Vector3 raw = ::Vector3Normalize({x, y, z});
        return Vector3(raw.x, raw.y, raw.z);
    }
    float Vector3::Dot(const Vector3& a, const Vector3& b) {
        return ::Vector3Dot({a.x, a.y, a.z}, {b.x, b.y, b.z});
    }
    Vector3 Vector3::Cross(const Vector3& a, const Vector3& b) {
        ::Vector3 raw = ::Vector3Cross({a.x, a.y, a.z}, {b.x, b.y, b.z});
        return Vector3(raw.x, raw.y, raw.z);
    }
    Vector3 Vector3::Lerp(const Vector3& start, const Vector3& end, float t) {
        ::Vector3 raw = ::Vector3Lerp({start.x, start.y, start.z}, {end.x, end.y, end.z}, t);
        return Vector3(raw.x, raw.y, raw.z);
    }



    // ==========================================
    // Quaternion Implementation
    // ==========================================
    
    Quaternion Quaternion::operator*(const Quaternion& b) const {
        ::Quaternion raw = ::QuaternionMultiply({x, y, z, w}, {b.x, b.y, b.z, b.w});
        return Quaternion(raw.x, raw.y, raw.z, raw.w);
    }
    Vector3 Quaternion::operator*(const Vector3& v) const {
        ::Vector3 raw = ::RotateVectorByQuaternion({v.x, v.y, v.z}, {x, y, z, w});
        return Vector3(raw.x, raw.y, raw.z);
    }
    void Quaternion::Normalize() {
        ::Quaternion raw = ::QuaternionNormalize({x, y, z, w});
        x = raw.x; y = raw.y; z = raw.z; w = raw.w;
    }
    Quaternion Quaternion::Normalized() const {
        ::Quaternion raw = ::QuaternionNormalize({x, y, z, w});
        return Quaternion(raw.x, raw.y, raw.z, raw.w);
    }
    void Quaternion::Inverse() {
        ::Quaternion raw = ::QuaternionInverse({x, y, z, w});
        x = raw.x; y = raw.y; z = raw.z; w = raw.w;
    }
    Quaternion Quaternion::Inversed() const {
        ::Quaternion raw = ::QuaternionInverse({x, y, z, w});
        return Quaternion(raw.x, raw.y, raw.z, raw.w);
    }
    Vector3 Quaternion::ToEuler() const {
        ::Vector3 raw = ::QuaternionToEuler({x, y, z, w});
        return Vector3(raw.x, raw.y, raw.z);
    }
    Quaternion Quaternion::Identity() {
        ::Quaternion raw = ::QuaternionIdentity();
        return Quaternion(raw.x, raw.y, raw.z, raw.w);
    }
    Quaternion Quaternion::FromEuler(float pitch, float yaw, float roll) {
        ::Quaternion raw = ::QuaternionFromEuler(pitch, yaw, roll);
        return Quaternion(raw.x, raw.y, raw.z, raw.w);
    }
    Quaternion Quaternion::FromEuler(const Vector3& euler) {
        ::Quaternion raw = ::QuaternionFromEuler(euler.x, euler.y, euler.z);
        return Quaternion(raw.x, raw.y, raw.z, raw.w);
    }
    Quaternion Quaternion::FromAxisAngle(float ax, float ay, float az, float angle) {
        ::Quaternion raw = ::QuaternionFromAxisAngle(ax, ay, az, angle);
        return Quaternion(raw.x, raw.y, raw.z, raw.w);
    }
    Quaternion Quaternion::FromAxisAngle(const Vector3& axis, float angle) {
        ::Quaternion raw = ::QuaternionFromAxisAngle(axis.x, axis.y, axis.z, angle);
        return Quaternion(raw.x, raw.y, raw.z, raw.w);
    }



    // ==========================================
    // Matrix4 Implementation
    // ==========================================
    
    // Helper to bridge our Matrix4 to ::Matrix4
    static inline ::Matrix4 ToCore(const Matrix4& m) {
        return ::Matrix4 {
            m.m0, m.m4, m.m8, m.m12,
            m.m1, m.m5, m.m9, m.m13,
            m.m2, m.m6, m.m10, m.m14,
            m.m3, m.m7, m.m11, m.m15 };
    }
    static inline Matrix4 FromCore(const ::Matrix4& raw) {
        Matrix4 m;
        m.m0 = raw.m0; m.m4 = raw.m4; m.m8 = raw.m8; m.m12 = raw.m12;
        m.m1 = raw.m1; m.m5 = raw.m5; m.m9 = raw.m9; m.m13 = raw.m13;
        m.m2 = raw.m2; m.m6 = raw.m6; m.m10 = raw.m10; m.m14 = raw.m14;
        m.m3 = raw.m3; m.m7 = raw.m7; m.m11 = raw.m11; m.m15 = raw.m15;
        return m;
    }

    Matrix4::Matrix4() {
        *this = Identity();
    }

    Matrix4 Matrix4::operator*(const Matrix4& b) const {
        return FromCore(::Matrix4Multiply(ToCore(*this), ToCore(b)));
    }
    Matrix4& Matrix4::operator*=(const Matrix4& b) {
        *this = FromCore(::Matrix4Multiply(ToCore(*this), ToCore(b)));
        return *this;
    }
    Vector3 Matrix4::operator*(const Vector3& v) const {
        ::Vector3 raw = ::Matrix4MultiplyVector3(ToCore(*this), {v.x, v.y, v.z});
        return Vector3(raw.x, raw.y, raw.z);
    }

    void Matrix4::Transpose() {
        *this = FromCore(::Matrix4Transpose(ToCore(*this)));
    }
    Matrix4 Matrix4::Transposed() const {
        return FromCore(::Matrix4Transpose(ToCore(*this)));
    }
    
    void Matrix4::Inverse() {
        *this = FromCore(::Matrix4Inverse(ToCore(*this)));
    }
    Matrix4 Matrix4::Inversed() const {
        return FromCore(::Matrix4Inverse(ToCore(*this)));
    }

    Quaternion Matrix4::ToQuaternion() const {
        ::Quaternion raw = ::QuaternionFromMatrix(ToCore(*this));
        return Quaternion(raw.x, raw.y, raw.z, raw.w);
    }
    void Matrix4::ToArray(float out[16]) const {
        ::Matrix4ToArray(ToCore(*this), out);
    }

    Matrix4 Matrix4::Identity() {
        return FromCore(::Matrix4Identity());
    }
    Matrix4 Matrix4::Translate(const Vector3& t) {
        return FromCore(::Matrix4Translate({t.x, t.y, t.z}));
    }
    Matrix4 Matrix4::Scale(const Vector3& s) {
        return FromCore(::Matrix4Scale({s.x, s.y, s.z}));
    }
    Matrix4 Matrix4::RotateX(float r) {
        return FromCore(::Matrix4RotateX(r));
    }
    
    Matrix4 Matrix4::CreateTransform(const Vector3& pos, const Quaternion& rot, const Vector3& scale) {
        return FromCore(::Matrix4CreateTransform({pos.x, pos.y, pos.z}, {rot.x, rot.y, rot.z, rot.w}, {scale.x, scale.y, scale.z}));
    }
    Matrix4 Matrix4::CreateView(const Vector3& pos, const Quaternion& rot) {
        return FromCore(::Matrix4CreateView({pos.x, pos.y, pos.z}, {rot.x, rot.y, rot.z, rot.w}));
    }
    Matrix4 Matrix4::LookAt(const Vector3& eye, const Vector3& target, const Vector3& up) {
        return FromCore(::Matrix4LookAt({eye.x, eye.y, eye.z}, {target.x, target.y, target.z}, {up.x, up.y, up.z}));
    }
    Matrix4 Matrix4::Perspective(float fovRadians, float aspect, float nearZ, float farZ) {
        return FromCore(::Matrix4Perspective(fovRadians, aspect, nearZ, farZ));
    }
    Matrix4 Matrix4::Ortho(float left, float right, float bottom, float top, float nearZ, float farZ) {
        return FromCore(::Matrix4Ortho(left, right, bottom, top, nearZ, farZ));
    }
}