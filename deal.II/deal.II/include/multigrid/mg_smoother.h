/*----------------------------   mg_smoother.h     ---------------------------*/
/*      $Id$                 */
#ifndef __mg_smoother_H
#define __mg_smoother_H
/*----------------------------   mg_smoother.h     ---------------------------*/


/**
 * Abstract base class for multigrid smoothers. It declares the interface
 * to smoothers and implements some functionality for setting the values
 * of vectors at interior boundaries (i.e. boundaries between differing
 * levels of the triangulation) to zero, by building a list of these degrees
 * of freedom's indices at construction time.
 *
 * @author Wolfgang Bangerth, Guido Kanschat, 1999
 */
class MGSmoother :  public Subcriptor 
{
  private:
				     /**
				      * Default constructor. Made private
				      * to prevent it being called, which
				      * is necessary since this could
				      * circumpass the set-up of the list
				      * if interior boundary dofs.
				      */
    MGSmoother ();
    
  public:

				     /**
				      * Constructor. This one sets clobbers
				      * the indices of the degrees of freedom
				      * on the interior boundaries between
				      * the different levels, which are
				      * needed by the function
				      * #set_zero_interior_boundaries#.
				      */
    template <int dim>
    MGSmoother (const MGDoFHandler<dim> &mg_dof);

				     /**
				      * Smooth the vector #u# on the given
				      * level. This function should set the
				      * interior boundary values of u to zero
				      * at the end of its work, so you may want
				      * to call #set_zero_interior_boundary#
				      * at the end of your derived function,
				      * or another function doing similar
				      * things.
				      */
    virtual void smooth (const unsigned int  level,
			 vector<float>      &u) const = 0;

				     /**
				      * Reset the values of the degrees of
				      * freedom on interior boundaries between
				      * different levels to zero in the given
				      * data vector #u#.
				      */
    void set_zero_interior_boundary (const unsigned int  level,
				     vector<float>      &u) const;

  private:
				     /**
				      * For each level, we store a list of
				      * degree of freedom indices which are
				      * located on interior boundaries between
				      * differing levels of the triangulation.
				      *
				      * These arrays are set by the constructor.
				      */
    vector<vector<int> > interior_boundary_dofs;
};

    


/*----------------------------   mg_smoother.h     ---------------------------*/
/* end of #ifndef __mg_smoother_H */
#endif
/*----------------------------   mg_smoother.h     ---------------------------*/
			 
