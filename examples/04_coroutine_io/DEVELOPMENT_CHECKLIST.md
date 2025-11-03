# Development Checklist: C++20 Coroutine io_uring Library

**Project**: io_uring_async Library Implementation  
**Purpose**: Step-by-step guide for complete implementation  
**Based On**: DESIGN.md, design_decisions.md, ARCHITECTURE_NOTES.md  
**Audience**: Developers implementing the library

---

## Overview

This checklist breaks down the implementation into logical phases, each with specific tasks. Complete each phase before moving to the next to maintain a solid foundation.

**Estimated Timeline**: 6-8 weeks for complete implementation  
**Prerequisites**: 
- Linux kernel 6.17+ development environment
- GCC 11+ or Clang 14+ with C++20 support
- liburing installed
- Understanding of C++20 coroutines

---

## Phase 0: Project Setup (Week 1, Days 1-2) ✅ COMPLETE

### 0.1 Repository Structure
- [x] Create directory structure following ARCHITECTURE_NOTES.md section 2.1
- [x] Set up include/io_uring_async/ directory hierarchy (core/, runtime/, io/)
- [x] Create src/ directory with matching subdirectories
- [x] Set up examples/, tests/ (unit/ and integration/) directories
- [x] Create docs/ directory for additional documentation

### 0.2 Build System
- [x] Create primary Makefile with C++20 requirements
- [x] Configure compiler flags (-std=c++20, -fcoroutines, -Wall, -Wextra)
- [x] Set up liburing linking (-luring)
- [x] Add Debug and Release build targets
- [x] Add sanitizer targets (asan, ubsan)
- [x] Add clean and test targets
- [ ] Optional: Create CMakeLists.txt for teams that prefer CMake

### 0.3 Development Infrastructure
- [x] Create .gitignore file (build artifacts, IDE files)
- [x] Set up version control ignore patterns
- [x] Create README.md skeleton
- [ ] Set up clang-format configuration for consistent style
- [ ] Create CONTRIBUTING.md with coding standards
- [ ] Set up issue templates (if using GitHub/GitLab)

### 0.4 Documentation Framework
- [x] Review all design documents for completeness
- [ ] Create API_SPEC.md outline (to be filled in during implementation)
- [ ] Set up Doxygen configuration (optional)
- [ ] Create examples/README.md explaining example structure

**Checkpoint**: ✅ Directory structure created, build system compiles and tests successfully

---

## Phase 1: Core Types (Week 1, Days 3-5) ✅ COMPLETE

### 1.1 Result<T, E> Type
- [x] Create include/io_uring_async/core/result.hpp header
- [x] Implement storage (std::variant) for value/error
- [x] Implement constructors (value, error, default)
- [x] Implement has_value(), value(), error() accessors
- [x] Add [[nodiscard]] attribute to class and key methods
- [x] Implement value_or() method
- [x] Implement operator bool() for boolean context
- [x] Add move semantics (move constructor, move assignment)
- [x] Implement monadic operations (map, and_then, or_else)
- [x] Add comparison operators (==, !=)
- [x] Create basic compilation tests for Result<T, E>
- [x] Test with different T and E types (int, Error, void)
- [x] Verify [[nodiscard]] enforcement
- [x] Implement Result<void, E> specialization with std::monostate

### 1.2 Error Types
- [x] Create include/io_uring_async/core/error.hpp
- [x] Define base Error struct (code, message)
- [x] Create factory methods (from_errno, from_io_uring_result, custom)
- [x] Implement error formatting methods (what())
- [x] Add error code constants for common errors
- [x] Document error handling conventions
- [x] Create basic tests for error types

### 1.3 Concepts (Optional but Recommended)
- [ ] Create include/io_uring_async/core/concepts.hpp
- [ ] Define Awaitable concept
- [ ] Define Executor concept
- [ ] Define IoOperation concept
- [ ] Add static_assert tests for concepts
- [ ] Document when and how to use concepts

**Checkpoint**: ✅ Result<T,E> and Error types implemented, tested, compiling without warnings

---

## Phase 2: Coroutine Runtime - Task<T> (Week 2) ✅ COMPLETE

### 2.1 Basic Task Structure
- [x] Create include/io_uring_async/core/task.hpp
- [x] Define Task<T> class template
- [x] Implement move-only semantics (delete copy, implement move)
- [x] Add coroutine_handle member variable
- [x] Implement destructor (destroy coroutine frame if owned)
- [x] Add done() and valid() query methods
- [x] Document ownership semantics

### 2.2 Promise Type
- [x] Define TaskPromise<T> class (detail::TaskPromise)
- [x] Implement get_return_object()
- [x] Implement initial_suspend() returning suspend_always (lazy evaluation)
- [x] Implement final_suspend() with FinalAwaiter for continuation
- [x] Implement return_value(T) for storing result
- [x] Implement unhandled_exception() for exception handling
- [x] Add result storage (std::variant<monostate, T, exception_ptr>)
- [x] Add continuation storage (coroutine_handle<>)
- [x] Document promise lifecycle

### 2.3 Awaitable Interface
- [x] Implement await_ready() (return false for lazy evaluation)
- [x] Implement await_suspend(coroutine_handle<>)
  - Store continuation in promise
  - Return this coroutine handle
- [x] Implement await_resume()
  - Extract result from promise
  - Rethrow exception if present
  - Return value or throw
- [x] Document suspension/resumption semantics

### 2.4 Task<void> Specialization
- [x] Create specialization for TaskPromise<void>
- [x] Adapt promise_type with return_void() instead of return_value()
- [x] Adjust await_resume() to return void
- [x] Test void specialization separately

### 2.5 Task Testing
- [x] Create test_task.cpp
- [x] Test basic coroutine creation and destruction
- [x] Test move semantics
- [x] Test Task<void> specialization
- [ ] Create comprehensive tests/unit/test_task.cpp
- [ ] Test suspension and resumption with executor
- [ ] Test value propagation through awaits
- [ ] Test exception propagation
- [ ] Test nested coroutine calls
- [ ] Test early return scenarios

**Checkpoint**: ✅ Task<T> implemented with lazy evaluation, compiles and basic tests pass

---

## Phase 3: io_uring Wrapper (Week 3, Days 1-3) ✅ COMPLETE

### 3.1 RAII io_uring Wrapper
- [x] Create include/io_uring_async/core/io_uring_wrapper.hpp
- [x] Define IoUring class with RAII semantics
- [x] Implement initialization with factory method create()
- [x] Implement destruction wrapper around io_uring_queue_exit()
- [x] Add move-only semantics
- [x] Add getter for io_uring pointer (raw())
- [x] Handle initialization errors properly with Result<T,E>
- [x] Document resource ownership

### 3.2 Operation Base
- [x] Create OpType enum for operation types
- [x] Create OperationData struct for user_data
- [x] Define operation ID type (uint64_t)
- [x] Document operation lifecycle

### 3.3 Specific Operations
- [x] Implement prep_accept (wraps io_uring_prep_accept)
- [x] Implement prep_send (wraps io_uring_prep_send)
- [x] Implement prep_recv (wraps io_uring_prep_recv)
- [x] Implement prep_read (wraps io_uring_prep_read)
- [x] Implement prep_write (wraps io_uring_prep_write)
- [x] Implement prep_close (wraps io_uring_prep_close)
- [x] Each operation prepares SQE correctly with type-safe std::span
- [x] Add validation and error checking
- [x] Document each operation type

### 3.4 Testing
- [x] Test io_uring initialization and cleanup
- [x] Test move semantics
- [x] Create test_io_uring.cpp with basic tests
- [ ] Test basic operation submission and completion
- [ ] Test error handling from io_uring
- [ ] Test resource cleanup on errors

**Checkpoint**: ✅ IoUring RAII wrapper complete, tested, compiles without warnings

---

## Phase 4: Executor Implementation (Week 3, Days 4-5 + Week 4, Days 1-2) ✅ COMPLETE

### 4.1 Basic Executor Structure
- [x] Create include/io_uring_async/runtime/executor.hpp
- [x] Define IoUringExecutor class
- [x] Add io_uring instance member (using IoUring wrapper)
- [x] Add operation ID counter (next_operation_id_)
- [x] Add pending operations map (operation_id -> coroutine_handle)
- [x] Add ready task queue
- [x] Implement factory method create() with queue depth parameter
- [x] Private constructor for RAII
- [x] Document executor lifecycle

### 4.2 Operation Submission
- [x] Implement register_operation() method
- [x] Implement unregister_operation() method
- [x] Implement allocate_operation_id() method
- [x] Store coroutine handle in pending map
- [x] Access to IoUring via ring() getter
- [ ] Implement full batching logic (currently basic submit)
- [ ] Add maybe_submit() helper
- [ ] Add force_submit() helper

### 4.3 Completion Processing
- [x] Implement process_completion() private method
- [x] Use io_uring_peek_cqe() and wait_cqe() for completions
- [x] Extract operation ID from CQE user_data
- [x] Extract result from CQE res field
- [x] Look up coroutine handle in pending map
- [x] Remove from pending map
- [x] Store result in operation_results_ map
- [x] Resume coroutine
- [x] Mark CQE as seen with io_uring_cqe_seen()

### 4.4 Event Loop
- [x] Implement run() method (run until all tasks complete)
- [x] Implement run_once() method for single iteration
- [ ] Implement run_for(duration) method
- [ ] Implement run_until(predicate) method
- [x] Event loop structure implemented:
  - Process ready tasks
  - Submit pending operations
  - Process completions
  - Wait if no ready work (io_uring_wait_cqe)
- [ ] Handle shutdown gracefully
- [ ] Add explicit stopping mechanism

### 4.5 Task Spawning
- [x] Implement spawn(Task<void>) method
- [x] Add spawned task to ready queue
- [ ] Ensure task execution in event loop (needs I/O awaitables)
- [ ] Handle task completion properly

### 4.6 Awaitable Integration
- [ ] Create include/io_uring_async/runtime/awaitable.hpp
- [ ] Define IoUringAwaitable<T> template
- [ ] Store executor pointer
- [ ] Store operation ID
- [ ] Implement await_ready() (return false)
- [ ] Implement await_suspend()
- [ ] Implement await_resume()

### 4.7 Testing
- [x] Test executor initialization
- [x] Create test_library.cpp with basic tests
- [ ] Test operation submission and completion
- [ ] Test batching behavior
- [ ] Test multiple concurrent operations
- [ ] Test event loop termination
- [ ] Test error propagation through executor
- [ ] Test spawn functionality
- [ ] Stress test with many operations

**Checkpoint**: ✅ Executor core structure complete, compiles without warnings, basic tests pass

---

## Phase 5: I/O Components (Week 4, Days 3-5 + Week 5, Days 1-2)

### 5.1 TcpListener
- [ ] Create include/io_uring_async/io/tcp_listener.hpp
- [ ] Create src/io/tcp_listener.cpp
- [ ] Define TcpListener class
- [ ] Implement static bind() method
  - Create socket
  - Set SO_REUSEADDR and SO_REUSEPORT
  - Bind to address
  - Start listening
  - Return Result<TcpListener, Error>
- [ ] Implement accept() coroutine
  - Submit accept operation to executor
  - co_await completion
  - Return TcpStream on success
- [ ] Implement port() getter
- [ ] Implement close() method
- [ ] Add RAII cleanup in destructor
- [ ] Document socket options and behavior

### 5.2 TcpStream
- [ ] Create include/io_uring_async/io/tcp_stream.hpp
- [ ] Create src/io/tcp_stream.cpp
- [ ] Define TcpStream class
- [ ] Store file descriptor and executor pointer
- [ ] Implement send(span<const byte>) coroutine
  - Submit send operation
  - co_await completion
  - Return bytes sent or error
- [ ] Implement receive(span<byte>) coroutine
  - Submit recv operation
  - co_await completion
  - Return bytes received or error
- [ ] Implement shutdown() method
- [ ] Implement close() method
- [ ] Add move-only semantics
- [ ] Add RAII cleanup in destructor

### 5.3 Socket Utilities
- [ ] Create address parsing helpers (string to sockaddr)
- [ ] Create address formatting helpers (sockaddr to string)
- [ ] Add IPv4 and IPv6 support
- [ ] Handle hostname resolution (if needed)

### 5.4 File I/O (Optional)
- [ ] Create include/io_uring_async/io/file.hpp
- [ ] Implement AsyncFile class
- [ ] Implement open() static method
- [ ] Implement read() and write() coroutines
- [ ] Implement close() method
- [ ] Add RAII cleanup

### 5.5 Testing
- [ ] Test TcpListener binding to various ports
- [ ] Test TcpListener accepting connections
- [ ] Test TcpStream send and receive
- [ ] Test connection closure
- [ ] Test error conditions (connection refused, etc.)
- [ ] Test with actual socket connections
- [ ] Integration test: simple echo server

**Checkpoint**: Can create TCP servers and handle connections

---

## Phase 6: Example Applications (Week 5, Days 3-5)

### 6.1 Echo Server Example
- [ ] Create examples/echo_server.cpp
- [ ] Implement handle_client() coroutine
  - Receive data
  - Send data back
  - Handle errors
  - Close connection
- [ ] Implement run_server() coroutine
  - Bind listener
  - Accept connections in loop
  - Spawn handler for each connection
- [ ] Implement main()
  - Create executor
  - Spawn server task
  - Run executor
- [ ] Add proper error handling and logging
- [ ] Document the example

### 6.2 Simple File Copy Example (Optional)
- [ ] Create examples/file_copy.cpp
- [ ] Implement async file read and write
- [ ] Show coroutine composition
- [ ] Add progress reporting
- [ ] Document the example

### 6.3 HTTP Server Example (Advanced, Optional)
- [ ] Create examples/http_server.cpp
- [ ] Implement basic HTTP request parsing
- [ ] Implement HTTP response generation
- [ ] Handle multiple concurrent connections
- [ ] Add static file serving
- [ ] Document the example

### 6.4 Build Examples
- [ ] Update main Makefile to build examples
- [ ] Create individual example Makefiles (or use subdirectory targets)
- [ ] Test that examples compile and run
- [ ] Verify memory safety with Valgrind

**Checkpoint**: Working examples demonstrating library usage

---

## Phase 7: Advanced Features (Week 6)

### 7.1 Buffer Management
- [ ] Implement registered buffer support
  - Create BufferPool class
  - Implement register_buffers() in executor
  - Add send_registered() and receive_registered() methods
  - Test zero-copy performance
- [ ] Document buffer strategies in API_SPEC.md
- [ ] Create example demonstrating registered buffers

### 7.2 Multishot Operations (Optional)
- [ ] Investigate multishot accept support
- [ ] Implement multishot accept if kernel version supports
- [ ] Add feature detection and graceful fallback
- [ ] Document multishot behavior

### 7.3 Timeout Support
- [ ] Implement timeout operations
- [ ] Create TimeoutAwaitable
- [ ] Implement operation-with-timeout pattern
- [ ] Test timeout behavior
- [ ] Document timeout usage

### 7.4 Performance Optimizations
- [ ] Profile executor with realistic workload
- [ ] Optimize hot paths (completion processing)
- [ ] Tune batch sizes based on benchmarks
- [ ] Consider cache line alignment for hot data structures
- [ ] Document performance characteristics

**Checkpoint**: Advanced features implemented and tested

---

## Phase 8: Comprehensive Testing (Week 7)

### 8.1 Unit Tests
- [ ] Complete tests/unit/test_result.cpp
- [ ] Complete tests/unit/test_task.cpp
- [ ] Complete tests/unit/test_executor.cpp
- [ ] Add tests for each I/O component
- [ ] Achieve >80% code coverage
- [ ] Test edge cases and error conditions
- [ ] Test with various data types

### 8.2 Integration Tests
- [ ] Create tests/integration/test_echo_server.cpp
- [ ] Test client-server communication
- [ ] Test multiple concurrent connections
- [ ] Test connection lifecycle
- [ ] Test error recovery
- [ ] Test graceful shutdown

### 8.3 Stress Tests
- [ ] Create stress test with 1000+ concurrent connections
- [ ] Test sustained high load
- [ ] Monitor memory usage
- [ ] Check for leaks with Valgrind
- [ ] Profile performance bottlenecks

### 8.4 Error Handling Tests
- [ ] Test behavior with invalid inputs
- [ ] Test network errors (connection refused, timeout)
- [ ] Test file descriptor exhaustion
- [ ] Test io_uring queue full scenarios
- [ ] Verify proper error propagation

### 8.5 Memory Safety
- [ ] Run all tests under Valgrind
- [ ] Run tests with AddressSanitizer
- [ ] Run tests with UndefinedBehaviorSanitizer
- [ ] Fix all reported issues
- [ ] Document memory safety guarantees

**Checkpoint**: Comprehensive test suite passing, no memory issues

---

## Phase 9: Documentation (Week 7-8)

### 9.1 API Documentation
- [ ] Complete API_SPEC.md with all public interfaces
- [ ] Document all public classes and methods
- [ ] Add usage examples for each major feature
- [ ] Document error conditions
- [ ] Document thread safety guarantees
- [ ] Document performance characteristics

### 9.2 User Guide
- [ ] Create README.md with:
  - Project overview
  - Installation instructions
  - Quick start guide
  - Link to examples
  - Link to API documentation
- [ ] Create GETTING_STARTED.md tutorial
- [ ] Create EXAMPLES.md explaining each example
- [ ] Add troubleshooting section

### 9.3 API Reference (Optional)
- [ ] Set up Doxygen comments in headers
- [ ] Generate Doxygen documentation
- [ ] Review generated docs for completeness
- [ ] Host documentation (if applicable)

### 9.4 Performance Guide
- [ ] Document performance characteristics
- [ ] Create benchmarking guide
- [ ] Document optimization techniques
- [ ] Compare with other libraries (if data available)
- [ ] Document kernel version requirements for features

### 9.5 Developer Documentation
- [ ] Update ARCHITECTURE_NOTES.md with implementation insights
- [ ] Document any deviations from original design
- [ ] Create debugging guide
- [ ] Document contribution guidelines
- [ ] Add code style guide

**Checkpoint**: Complete documentation for users and developers

---

## Phase 10: Polish and Release Preparation (Week 8)

### 10.1 Code Quality
- [ ] Run clang-format on all source files
- [ ] Run clang-tidy and fix issues
- [ ] Review all compiler warnings
- [ ] Apply RAII consistently throughout
- [ ] Ensure consistent error handling
- [ ] Review and improve comments

### 10.2 Build System Polish
- [ ] Test Makefile build on clean system
- [ ] Create install and uninstall targets
- [ ] Test on different distributions
- [ ] Document build requirements
- [ ] Ensure examples build correctly
- [ ] Optional: Test CMake build if provided

### 10.3 Platform Testing
- [ ] Test on different Linux distributions
- [ ] Test with different kernel versions (6.17+, 5.19+)
- [ ] Test with GCC 11, 12, 13
- [ ] Test with Clang 14, 15, 16
- [ ] Document tested configurations

### 10.4 Example Refinement
- [ ] Ensure all examples compile
- [ ] Verify examples run without errors
- [ ] Add comments explaining key points
- [ ] Make examples self-contained
- [ ] Add README to each example

### 10.5 Performance Validation
- [ ] Run benchmark suite
- [ ] Compare against design goals
- [ ] Document actual performance characteristics
- [ ] Identify optimization opportunities
- [ ] Update documentation with benchmark results

### 10.6 Release Checklist
- [ ] Version numbering (use semantic versioning)
- [ ] Create CHANGELOG.md
- [ ] Create LICENSE file
- [ ] Tag release in version control
- [ ] Create release notes
- [ ] Package for distribution (if applicable)

**Checkpoint**: Library ready for release

---

## Phase 11: Post-Release (Ongoing)

### 11.1 Community Engagement
- [ ] Announce release (blog post, forums, etc.)
- [ ] Set up issue tracker
- [ ] Monitor for bug reports
- [ ] Respond to questions
- [ ] Accept contributions

### 11.2 Maintenance
- [ ] Fix reported bugs
- [ ] Update for new kernel versions
- [ ] Update for new C++ standards
- [ ] Keep dependencies current
- [ ] Maintain documentation

### 11.3 Future Enhancements
- [ ] Implement features from "Future Directions" in DESIGN.md
- [ ] HTTP/2 support
- [ ] TLS integration
- [ ] Additional I/O operations
- [ ] Multi-threaded executor variant
- [ ] Performance improvements

---

## Appendix A: Development Best Practices

### Code Review Checklist
For each completed feature, verify:
- [ ] Follows design decisions from design_decisions.md
- [ ] Matches architectural guidance from ARCHITECTURE_NOTES.md
- [ ] Has comprehensive unit tests
- [ ] Has integration tests (if applicable)
- [ ] Documented in API_SPEC.md
- [ ] No memory leaks (Valgrind clean)
- [ ] No undefined behavior (sanitizers clean)
- [ ] Follows coding style guide
- [ ] Appropriate error handling
- [ ] RAII used consistently

### Testing Strategy
- Write tests before or alongside implementation (TDD encouraged)
- Test both success and failure paths
- Test edge cases and boundary conditions
- Use realistic data in integration tests
- Profile and benchmark critical paths
- Run tests frequently during development

### Documentation Strategy
- Document public APIs as you write them
- Keep design docs updated with decisions
- Add examples as features are completed
- Review documentation for clarity
- Get feedback from potential users

---

## Appendix B: Estimated Effort

| Phase | Duration | Dependencies | Risk |
|-------|----------|--------------|------|
| 0. Project Setup | 2 days | None | Low |
| 1. Core Types | 3 days | Phase 0 | Low |
| 2. Task<T> | 5 days | Phase 1 | Medium |
| 3. io_uring Wrapper | 3 days | Phase 1 | Medium |
| 4. Executor | 5 days | Phases 2-3 | High |
| 5. I/O Components | 5 days | Phase 4 | Medium |
| 6. Examples | 3 days | Phase 5 | Low |
| 7. Advanced Features | 5 days | Phase 5 | Medium |
| 8. Testing | 5 days | All previous | Medium |
| 9. Documentation | 5 days | All previous | Low |
| 10. Polish | 3 days | All previous | Low |
| **Total** | **~44 days** | | |

**Note**: This is for one experienced developer. Adjust based on team size and experience.

---

## Appendix C: Risk Mitigation

### High-Risk Areas
1. **Coroutine lifetime management**: Test extensively, use Valgrind
2. **io_uring operation tracking**: Use comprehensive logging in debug builds
3. **Concurrent operation handling**: Stress test with many operations
4. **Error propagation**: Test all error paths thoroughly

### Mitigation Strategies
- Start with simple cases, add complexity gradually
- Use sanitizers from day one
- Write tests early and run them often
- Get code reviews from C++ coroutine experts
- Profile early to catch performance issues
- Document assumptions and invariants

---

## Appendix D: Success Criteria

The implementation is complete and successful when:

1. **Functionality**: All features from DESIGN.md implemented
2. **Quality**: All tests pass, no memory leaks, no undefined behavior
3. **Performance**: Meets targets from DESIGN.md section 6.1
4. **Documentation**: Complete API documentation and user guides
5. **Examples**: Multiple working examples demonstrating key features
6. **Usability**: Echo server example under 100 lines (per design goal)
7. **Portability**: Works on supported Linux kernels and compilers

---

## References

- DESIGN.md: High-level architecture and design decisions
- ARCHITECTURE_NOTES.md: Technical implementation guidance
- design_decisions.md: Documented architectural choices
- API_SPEC.md: Detailed API contracts (to be created during development)
