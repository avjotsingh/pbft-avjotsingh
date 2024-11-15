// #include <vector>
// #include <mutex>
// #include <shared_mutex>
// #include <stdexcept>

// namespace types {
//     template <typename T>
//     class ThreadSafeVector {
//     private:
//         std::vector<T> vec;
//         mutable std::shared_mutex mtx;

//     public:
//         // Adds an element to the end of the vector
//         void push_back(const T& value) {
//             std::unique_lock<std::shared_mutex> lock(mtx);
//             vec.push_back(value);
//         }

//         // Erases an element at the specified index
//         template< class InputIt >
//         void erase(InputIt begin, InputIt end) {
//             std::unique_lock<std::shared_mutex> lock(mtx);
//             vec.erase(begin, end);
//         }

//         // Inserts an element at the specified index
//         template< class InputIt >
//         iterator insert(const_iterator pos, InputIt first, InputIt last) {
//             std::unique_lock<std::shared_mutex> lock(mtx);
//             vec.insert(vec.begin(), first, last);
//         }

//         // Retrieves the element at the specified index
//         T get(size_t index) const {
//             std::shared_lock<std::shared_mutex> lock(mtx);
//             if (index < vec.size()) {
//                 return vec[index];
//             }
//             throw std::out_of_range("Index out of range");
//         }

//         // Gets the size of the vector
//         size_t size() const {
//             std::shared_lock<std::shared_mutex> lock(mtx);
//             return vec.size();
//         }
//     };

// }