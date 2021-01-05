////////////////////////////////////////////////////////////////////////////////
// The Loki Library
// Copyright (c) 2001 by Andrei Alexandrescu
// This code accompanies the book:
// Alexandrescu, Andrei. "Modern C++ Design: Generic Programming and Design
//     Patterns Applied". Copyright (c) 2001. Addison-Wesley.
// Permission to use, copy, modify, distribute and sell this software for any
//     purpose is hereby granted without fee, provided that the above copyright
//     notice appear in all copies and that both that copyright notice and this
//     permission notice appear in supporting documentation.
// The author or Addison-Wesley Longman make no representations about the
//     suitability of this software for any purpose. It is provided "as is"
//     without express or implied warranty.
////////////////////////////////////////////////////////////////////////////////

// $Header: /cvsroot-fuse/loki-lib/loki/include/loki/Visitor.h,v 1.7 2006/01/16 19:05:09 rich_sposato Exp $

///  \defgroup VisitorGroup Visitor

#ifndef LOKI_VISITOR_INC_
#define LOKI_VISITOR_INC_

namespace Loki
{

////////////////////////////////////////////////////////////////////////////////
/// \class BaseVisitor
///
/// \ingroup VisitorGroup
/// The base class of any Acyclic Visitor
////////////////////////////////////////////////////////////////////////////////

    class BaseVisitor
    {
    public:
        virtual ~BaseVisitor() {}
    };

////////////////////////////////////////////////////////////////////////////////
/// \class Visitor
///
/// \ingroup VisitorGroup
/// The building block of Acyclic Visitor
///
/// \par Usage
///
/// Defining the visitable class:
///
/// \code
/// class RasterBitmap : public BaseVisitable<>
/// {
/// public:
///     LOKI_DEFINE_VISITABLE()
/// };
/// \endcode
///
/// Way 1 to define a visitor:
/// \code
/// class SomeVisitor :
///     public BaseVisitor // required
///     public Visitor<RasterBitmap>,
///     public Visitor<Paragraph>
/// {
/// public:
///     void Visit(RasterBitmap&); // visit a RasterBitmap
///     void Visit(Paragraph &);   // visit a Paragraph
/// };
/// \endcode
///
/// Way 2 to define the visitor:
/// \code
/// class SomeVisitor :
///     public BaseVisitor // required
///     public Visitor<LOKI_TYPELIST_2(RasterBitmap, Paragraph)>
/// {
/// public:
///     void Visit(RasterBitmap&); // visit a RasterBitmap
///     void Visit(Paragraph &);   // visit a Paragraph
/// };
/// \endcode
///
/// Way 3 to define the visitor:
/// \code
/// class SomeVisitor :
///     public BaseVisitor // required
///     public Visitor<Seq<RasterBitmap, Paragraph>::Type>
/// {
/// public:
///     void Visit(RasterBitmap&); // visit a RasterBitmap
///     void Visit(Paragraph &);   // visit a Paragraph
/// };
/// \endcode
///
/// \par Using const visit functions:
///
/// Defining the visitable class (true for const):
///
/// \code
/// class RasterBitmap : public BaseVisitable<void, DefaultCatchAll, true>
/// {
/// public:
///     LOKI_DEFINE_CONST_VISITABLE()
/// };
/// \endcode
///
/// Defining the visitor which only calls const member functions:
/// \code
/// class SomeVisitor :
///     public BaseVisitor // required
///     public Visitor<RasterBitmap, void, true>,
/// {
/// public:
///     void Visit(const RasterBitmap&); // visit a RasterBitmap by a const member function
/// };
/// \endcode
///
/// \par Example:
///
/// test/Visitor/main.cpp
////////////////////////////////////////////////////////////////////////////////

    template <class T, typename R = void, bool constVisit = false>
    class Visitor;

    template <class T, typename R>
    class Visitor<T, R, false>
    {
    public:
        typedef R ReturnType;
        typedef T ParamType;
        virtual ~Visitor() {}
        virtual ReturnType visit(ParamType&) = 0;
    };

    template <class T, typename R>
    class Visitor<T, R, true>
    {
    public:
        typedef R ReturnType;
        typedef const T ParamType;
        virtual ~Visitor() {}
        virtual ReturnType visit(ParamType&) = 0;
    };

////////////////////////////////////////////////////////////////////////////////
// class template BaseVisitable
////////////////////////////////////////////////////////////////////////////////

template <typename R, typename Visited>
struct DefaultCatchAll
{
    static R onUnknownVisitor(Visited&, BaseVisitor&)
    { return R(); }
};

////////////////////////////////////////////////////////////////////////////////
// class template BaseVisitable
////////////////////////////////////////////////////////////////////////////////

    template
    <
        typename R = void,
        template <typename, class> class CatchAll = DefaultCatchAll,
        bool constVisitable = false
    >
    class BaseVisitable;

    template<typename R,template <typename, class> class CatchAll>
    class BaseVisitable<R, CatchAll, false>
    {
    public:
        typedef R ReturnType;
        virtual ~BaseVisitable() {}
        virtual ReturnType accept(BaseVisitor&) = 0;

    protected: // give access only to the hierarchy
        template <class T>
        static ReturnType acceptImpl(T& visited, BaseVisitor& guest)
        {
            // Apply the Acyclic Visitor
            if (Visitor<T,R>* p = dynamic_cast<Visitor<T,R>*>(&guest))
            {
                return p->visit(visited);
            }
            return CatchAll<R, T>::onUnknownVisitor(visited, guest);
        }
    };

    template<typename R,template <typename, class> class CatchAll>
    class BaseVisitable<R, CatchAll, true>
    {
    public:
        typedef R ReturnType;
        virtual ~BaseVisitable() {}
        virtual ReturnType accept(BaseVisitor&) const = 0;

    protected: // give access only to the hierarchy
        template <class T>
        static ReturnType acceptImpl(const T& visited, BaseVisitor& guest)
        {
            // Apply the Acyclic Visitor
            if (Visitor<T,R,true>* p = dynamic_cast<Visitor<T,R,true>*>(&guest))
            {
                return p->visit(visited);
            }
            return CatchAll<R, T>::onUnknownVisitor(const_cast<T&>(visited), guest);
        }
    };


////////////////////////////////////////////////////////////////////////////////
/// \def LOKI_DEFINE_VISITABLE()
/// \ingroup VisitorGroup
/// Put it in every class that you want to make visitable
/// (in addition to deriving it from BaseVisitable<R>)
////////////////////////////////////////////////////////////////////////////////

#define LOKI_DEFINE_VISITABLE() \
    virtual ReturnType accept(::Loki::BaseVisitor& guest) \
    { return acceptImpl(*this, guest); }

////////////////////////////////////////////////////////////////////////////////
/// \def LOKI_DEFINE_CONST_VISITABLE()
/// \ingroup VisitorGroup
/// Put it in every class that you want to make visitable by const member
/// functions (in addition to deriving it from BaseVisitable<R>)
////////////////////////////////////////////////////////////////////////////////

#define LOKI_DEFINE_CONST_VISITABLE() \
    virtual ReturnType accept(::Loki::BaseVisitor& guest) const \
    { return acceptImpl(*this, guest); }

} // namespace Loki

////////////////////////////////////////////////////////////////////////////////
// Change log:
// March     20, ????: add default argument DefaultCatchAll to BaseVisitable
// June      20, 2001: ported by Nick Thurn to gcc 2.95.3. Kudos, Nick!!!
// September 28, 2004: replaced Loki:: with ::Loki:: in DEFINE_VISITABLE
// January    2, 2006: add support for visiting constant member functions, Peter Kümmel
////////////////////////////////////////////////////////////////////////////////

#endif // VISITOR_INC_

// $Log: Visitor.h,v $
// Revision 1.7  2006/01/16 19:05:09  rich_sposato
// Added cvs keywords.
//
