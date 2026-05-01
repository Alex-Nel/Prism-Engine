#pragma once



namespace Prism
{

    // ==========================================
    // Vector2 Wrapper
    // ==========================================

    struct Vector2
    {
        float x, y;

        Vector2() : x(0.0f), y(0.0f) {}
        Vector2(float _x, float _y) : x(_x), y(_y) {}



        // --- Operator overloads ---

        inline Vector2 operator+(const Vector2& b) const { return Vector2(x + b.x, y + b.y); }
        inline Vector2 operator-(const Vector2& b) const { return Vector2(x - b.x, y - b.y); }
        inline Vector2 operator*(float s) const { return Vector2(x * s, y * s); }
        
        inline Vector2& operator+=(const Vector2& b) { x += b.x; y += b.y; return *this; }
        inline Vector2& operator-=(const Vector2& b) { x -= b.x; y -= b.y; return *this; }
        inline Vector2& operator*=(float s) { x *= s; y *= s; return *this; }



        // --- Non operator functions ---

        void Normalize();
        Vector2 Normalized() const;

        static float Dot(const Vector2& a, const Vector2& b);
        static Vector2 Lerp(const Vector2& start, const Vector2& end, float t);
    };



    // ==========================================
    // Vector3 Wrapper
    // ==========================================

    struct Vector3
    {
        float x, y, z;

        Vector3() : x(0.0f), y(0.0f), z(0.0f) {}
        Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}



        // --- Operator overloads ---
        
        inline Vector3 operator+(const Vector3& b) const { return Vector3(x + b.x, y + b.y, z + b.z); }
        inline Vector3 operator-(const Vector3& b) const { return Vector3(x - b.x, y - b.y, z - b.z); }
        inline Vector3 operator*(float s) const { return Vector3(x * s, y * s, z * s); }
        
        inline Vector3& operator+=(const Vector3& b) { x += b.x; y += b.y; z += b.z; return *this; }
        inline Vector3& operator-=(const Vector3& b) { x -= b.x; y -= b.y; z -= b.z; return *this; }
        inline Vector3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }


        
        // --- Non operator functions ---
        
        void Normalize();
        Vector3 Normalized() const;

        static float Dot(const Vector3& a, const Vector3& b);
        static Vector3 Cross(const Vector3& a, const Vector3& b);
        static Vector3 Lerp(const Vector3& start, const Vector3& end, float t);
    };


    
    // ==========================================
    // Quaternion Wrapper
    // ==========================================

    struct Quaternion
    {
        float x, y, z, w;

        Quaternion() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
        Quaternion(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}


        // --- Operator overloads ---

        Quaternion operator*(const Quaternion& b) const;
        Vector3 operator*(const Vector3& v) const;

        void Normalize();
        Quaternion Normalized() const;
        void Inverse();
        Quaternion Inversed() const;
        Vector3 ToEuler() const;


        
        // --- Class static functions ---
        
        static Quaternion Identity();
        static Quaternion FromEuler(float pitch, float yaw, float roll);
        static Quaternion FromEuler(const Vector3& euler);
        static Quaternion FromAxisAngle(float ax, float ay, float az, float angle);
        static Quaternion FromAxisAngle(const Vector3& axis, float angle);
    };



    // ==========================================
    // MATRIX 4 WRAPPER
    // ==========================================

    struct Matrix4
    {
        // Standard column-major 4x4 matrix layout

        float m0, m4, m8, m12;
        float m1, m5, m9, m13;
        float m2, m6, m10, m14;
        float m3, m7, m11, m15;

        Matrix4();



        // --- Operator overloads ---

        Matrix4 operator*(const Matrix4& b) const;
        Matrix4& operator*=(const Matrix4& b);
        Vector3 operator*(const Vector3& v) const; // Allows: Vector3 worldPos = worldMatrix * localPos;



        // --- Non operator functions ---
        
        void Transpose();
        Matrix4 Transposed() const;
        void Inverse();
        Matrix4 Inversed() const;
        Quaternion ToQuaternion() const;
        void ToArray(float out[16]) const;



        // --- Class static functions ---
        
        static Matrix4 Identity();
        static Matrix4 Translate(const Vector3& t);
        static Matrix4 Scale(const Vector3& s);
        static Matrix4 RotateX(float r);
        static Matrix4 CreateTransform(const Vector3& pos, const Quaternion& rot, const Vector3& scale);
        static Matrix4 CreateView(const Vector3& pos, const Quaternion& rot);
        static Matrix4 LookAt(const Vector3& eye, const Vector3& target, const Vector3& up);
        static Matrix4 Perspective(float fovRadians, float aspect, float nearZ, float farZ);
        static Matrix4 Ortho(float left, float right, float bottom, float top, float nearZ, float farZ);
    };
}