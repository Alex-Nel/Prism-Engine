#pragma once

extern "C" {
    #include "Vector3.h"
    #include "Vector2.h"
    #include "Quaternion.h"
    #include "Matrix4.h"
}

namespace Prism
{

    // ==========================================
    // Vector2 Wrapper
    // ==========================================
    struct Vector2 : public ::Vector2
    {
        // --- Constructors ---
        Vector2() { x = 0.0f; y = 0.0f; }
        Vector2(float _x, float _y) { x = _x; y = _y; }

        Vector2(const ::Vector2& raw) { x = raw.x; y = raw.y; }
        operator ::Vector2() const { return ::Vector2{x, y}; }

        // --- Operator overloads ---
        Vector2 operator+(const Vector2& b) const {
            return Vector2Add(*this, b);
        }
        Vector2& operator+=(const Vector2& b) {
            *this = Vector2Add(*this, b);
            return *this;
        }
        Vector2 operator-(const Vector2& b) const {
            return Vector2Subtract(*this, b);
        }
        Vector2& operator-=(const Vector2& b) {
            *this = Vector2Subtract(*this, b);
            return *this;
        }
        Vector2 operator*(float s) const {
            return Vector2Scale(*this, s);
        }


        // --- Non operator functions ---
        void Normalize() {
            *this = Vector2Normalize(*this);
        }
        Vector2 Normalized() const {
            return Vector2Normalize(*this);
        }


        // --- Class static functions ---
        static float Dot(const Vector2& a, const Vector2& b) {
            return Vector2Dot(a, b);
        }
        static Vector2 Lerp(const Vector2& start, const Vector2& end, float t) {
            return Vector2Lerp(start, end, t);
        }
    };



    // ==========================================
    // Vector3 Wrapper
    // ==========================================
    struct Vector3 : public ::Vector3
    {
        // --- Constructors ---   
        Vector3() { x = 0.0f; y = 0.0f; z = 0.0f; }
        Vector3(float _x, float _y, float _z) { x = _x; y = _y; z = _z; }

        Vector3(const ::Vector3& raw) { x = raw.x; y = raw.y; z = raw.z; }
        operator ::Vector3() const { return ::Vector3{x, y, z}; }


        // --- Operator overloads ---
        Vector3 operator+(const Vector3& b) const {
            return Vector3Add(*this, b);
        }
        Vector3& operator+=(const Vector3& b) {
            *this = Vector3Add(*this, b);
            return *this;
        }
        Vector3 operator-(const Vector3& b) const {
            return Vector3Subtract(*this, b);
        }
        Vector3& operator-=(const Vector3& b) {
            *this = Vector3Subtract(*this, b);
            return *this;
        }
        Vector3 operator*(float s) const {
            return Vector3Scale(*this, s);
        }


        // --- Non operator functions ---
        void Normalize() {
            *this = Vector3Normalize(*this);
        }
        Vector3 Normalized() const {
            return Vector3Normalize(*this);
        }


        // --- Class static functions ---
        static float Dot(const Vector3& a, const Vector3& b) {
            return Vector3Dot(a, b);
        }
        static Vector3 Cross(const Vector3& a, const Vector3& b) {
            return Vector3Cross(a, b);
        }
        static Vector3 Lerp(const Vector3& start, const Vector3& end, float t) {
            return Vector3Lerp(start, end, t);
        }
    };


    
    // ==========================================
    // Quaternion Wrapper
    // ==========================================
    struct Quaternion : public ::Quaternion
    {
        // --- Constructors ---
        Quaternion() { x = 0.0f; y = 0.0f; z = 0.0f; w = 1.0f; }
        Quaternion(float _x, float _y, float _z, float _w) { x = _x; y = _y; z = _z; w = _w; }
        
        Quaternion(const ::Quaternion& raw) { x = raw.x; y = raw.y; z = raw.z; w = raw.w; }
        operator ::Quaternion() const { return ::Quaternion{x, y, z, w}; }


        // --- Operator overloads ---
        // Allows: q1 * q2 (Combining rotations)
        Quaternion operator*(const Quaternion& b) const {
            return QuaternionMultiply(*this, b);
        }
        // Allows: Vector3 rotatedDir = myQuat * forwardVec; 
        Vector3 operator*(const Vector3& v) const {
            return RotateVectorByQuaternion(v, *this);
        }


        // --- Non operator functions ---
        void Normalize() {
            *this = QuaternionNormalize(*this);
        }
        Quaternion Normalized() const {
            return QuaternionNormalize(*this);
        }
        void Inverse() {
            *this = QuaternionInverse(*this);
        }
        Quaternion Inversed() const {
            return QuaternionInverse(*this);
        }
        Vector3 ToEuler() const {
            return QuaternionToEuler(*this);
        }


        // --- Class static functions ---
        static Quaternion Identity() {
            return QuaternionIdentity();
        }
        static Quaternion FromEuler(float pitch, float yaw, float roll) {
            return QuaternionFromEuler(pitch, yaw, roll);
        }
        static Quaternion FromEuler(const Vector3& euler) {
            return QuaternionFromEuler(euler.x, euler.y, euler.z);
        }
        static Quaternion FromAxisAngle(float ax, float ay, float az, float angle) {
            return QuaternionFromAxisAngle(ax, ay, az, angle);
        }
        static Quaternion FromAxisAngle(const Vector3& axis, float angle) {
            return QuaternionFromAxisAngle(axis.x, axis.y, axis.z, angle);
        }
    };



    // ==========================================
    // MATRIX 4 WRAPPER
    // ==========================================
    struct Matrix4 : public ::Matrix4
    {
        // --- Constructors ---    
        Matrix4() { *this = Matrix4Identity(); }
        Matrix4(const ::Matrix4& raw) { *this = raw; }
        operator ::Matrix4() const { return *this; }


        // --- Operator overloads ---
        // Allows: proj * view * model
        Matrix4 operator*(const Matrix4& b) const {
            return Matrix4Multiply(*this, b);
        }
        Matrix4& operator*=(const Matrix4& b) {
            *this = Matrix4Multiply(*this, b);
            return *this;
        }
        // Allows: Vector3 worldPos = worldMatrix * localPos;
        Vector3 operator*(const Vector3& v) const {
            return Matrix4MultiplyVector3(*this, v);
        }


        // --- Non operator functions ---
        void Transpose() {
            *this = Matrix4Transpose(*this);
        }
        Matrix4 Transposed() const {
            return Matrix4Transpose(*this);
        }

        void Inverse() {
            *this = Matrix4Inverse(*this);
        }
        Matrix4 Inversed() const {
            return Matrix4Inverse(*this);
        }

        Quaternion ToQuaternion() const {
            return QuaternionFromMatrix(*this);
        }
        
        void ToArray(float out[16]) const {
            Matrix4ToArray(*this, out);
        }


        // --- Class static functions ---
        static Matrix4 Identity() {
            return Matrix4Identity();
        }
        static Matrix4 Translate(const Vector3& t) {
            return Matrix4Translate(t);
        }
        static Matrix4 Scale(const Vector3& s) {
            return Matrix4Scale(s);
        }
        static Matrix4 RotateX(float r) {
            return Matrix4RotateX(r);
        }
        
        static Matrix4 CreateTransform(const Vector3& pos, const Quaternion& rot, const Vector3& scale) {
            return Matrix4CreateTransform(pos, rot, scale);
        }
        static Matrix4 CreateView(const Vector3& pos, const Quaternion& rot) {
            return Matrix4CreateView(pos, rot);
        }
        static Matrix4 LookAt(const Vector3& eye, const Vector3& target, const Vector3& up) {
            return Matrix4LookAt(eye, target, up);
        }
        
        static Matrix4 Perspective(float fovRadians, float aspect, float nearZ, float farZ) {
            return Matrix4Perspective(fovRadians, aspect, nearZ, farZ);
        }
        static Matrix4 Ortho(float left, float right, float bottom, float top, float nearZ, float farZ) {
            return Matrix4Ortho(left, right, bottom, top, nearZ, farZ);
        }
    };
}