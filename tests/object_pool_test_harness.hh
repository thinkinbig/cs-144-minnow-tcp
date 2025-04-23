#pragma once

#include "common.hh"
#include "object_pool.hh"
#include <functional>
#include <string>

template<typename T>
class ObjectPoolTestStep : public TestStep<GeneralObjectPool<T>>
{
public:
  using TestStep<GeneralObjectPool<T>>::TestStep;
  virtual std::string description() const = 0;
  std::string str() const override { return description(); }
  uint8_t color() const override { return 0; }
};

template<typename T>
class Borrow : public ObjectPoolTestStep<T>
{
public:
  Borrow() : ref_() {}
  void execute( GeneralObjectPool<T>& pool ) const override
  {
    const_cast<Borrow<T>*>( this )->ref_ = pool.borrow();
  }
  std::string description() const override { return "borrow"; }
  Ref<T>& get_ref() { return ref_; }

private:
  Ref<T> ref_;
};

template<typename T>
class Return : public ObjectPoolTestStep<T>
{
public:
  explicit Return( const T& obj ) : obj_( obj ) {}
  void execute( GeneralObjectPool<T>& pool ) const override { pool.return_object( obj_ ); }
  std::string description() const override { return "return"; }

private:
  T obj_;
};

template<typename T>
class PoolSize : public ExpectNumber<GeneralObjectPool<T>, size_t>
{
public:
  explicit PoolSize( size_t expected ) : ExpectNumber<GeneralObjectPool<T>, size_t>( expected ) {}
  size_t value( const GeneralObjectPool<T>& pool ) const override { return pool.size(); }
  std::string description() const override { return "pool size"; }
  std::string name() const override { return "pool size"; }
};

template<typename T>
class AvailableCount : public ExpectNumber<GeneralObjectPool<T>, size_t>
{
public:
  explicit AvailableCount( size_t expected ) : ExpectNumber<GeneralObjectPool<T>, size_t>( expected ) {}
  size_t value( const GeneralObjectPool<T>& pool ) const override { return pool.available_count(); }
  std::string description() const override { return "available count"; }
  std::string name() const override { return "available count"; }
};

template<typename T>
class BorrowedCount : public ExpectNumber<GeneralObjectPool<T>, size_t>
{
public:
  explicit BorrowedCount( size_t expected ) : ExpectNumber<GeneralObjectPool<T>, size_t>( expected ) {}
  size_t value( const GeneralObjectPool<T>& pool ) const override { return pool.borrowed_count(); }
  std::string description() const override { return "borrowed count"; }
  std::string name() const override { return "borrowed count"; }
};

template<typename T>
class ObjectPoolTestHarness : public TestHarness<GeneralObjectPool<T>>
{
public:
  ObjectPoolTestHarness( std::string test_name,
                         typename GeneralObjectPool<T>::Creator creator,
                         typename GeneralObjectPool<T>::Resetter resetter,
                         size_t pre_allocated = 0,
                         size_t max_size = 0 )
    : TestHarness<GeneralObjectPool<T>>( std::move( test_name ),
                                         "object_pool",
                                         GeneralObjectPool<T>( creator, resetter, pre_allocated, max_size ) )
  {}

  Ref<T> borrow()
  {
    Borrow<T> borrow_step;
    this->execute( borrow_step );
    return borrow_step.get_ref();
  }

  void return_object( const T& obj ) { this->execute( Return<T>( obj ) ); }

  void expect_size( size_t expected ) { this->execute( PoolSize<T>( expected ) ); }

  void expect_available( size_t expected ) { this->execute( AvailableCount<T>( expected ) ); }

  void expect_borrowed( size_t expected ) { this->execute( BorrowedCount<T>( expected ) ); }
};